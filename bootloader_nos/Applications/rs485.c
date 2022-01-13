#include "main.h"
#include "task_run.h"
#include "ringbuffer.h"
#include "rs485.h"
#include "common.h"
#include "agile_modbus.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "rs485"
#define DBG_LEVEL        DBG_LOG
#include "dbg_log.h"

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;

static struct rt_ringbuffer _rx_rb = {0};
static uint8_t _rx_rb_buf[1024];
static uint32_t _rx_index = 0;
static uint32_t _rx_tick_timeout = 0;
static uint32_t _byte_timeout = 0;
static uint32_t _tx_tick_timeout = 0;
static uint32_t _tx_complete = 0;
static uint8_t _ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
static uint8_t _ctx_read_buf1[AGILE_MODBUS_MAX_ADU_LENGTH];

static agile_modbus_rtu_t _ctx_rtu;
static int _modbus_step[MODBUS_MODE_MAX] = {0};

#define RS485_TX_EN() HAL_GPIO_WritePin(RS485_RE_GPIO_Port, RS485_RE_Pin, GPIO_PIN_SET)
#define RS485_RX_EN() HAL_GPIO_WritePin(RS485_RE_GPIO_Port, RS485_RE_Pin, GPIO_PIN_RESET)

static int rs485_rx_rb_update(uint32_t size)
{
    if (rt_ringbuffer_put_update(&_rx_rb, size) != size)
        return -1;

    return 0;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    HAL_UART_AbortTransmit(&huart2);
    RS485_RX_EN();
    _tx_complete = 1;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint32_t recv_len = 0;

    uint32_t index = __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    if (index >= _rx_index) {
        recv_len = _rx_index + _rx_rb.buffer_size - index;
        _rx_index = index;
    }

    if (recv_len > 0) {
        rs485_rx_rb_update(recv_len);
        _rx_tick_timeout = HAL_GetTick() + _byte_timeout;
    }
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    uint32_t recv_len = 0;

    uint32_t index = __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    if (index < _rx_index) {
        recv_len = _rx_index - index;
        _rx_index = index;
    }

    if (recv_len > 0) {
        rs485_rx_rb_update(recv_len);
        _rx_tick_timeout = HAL_GetTick() + _byte_timeout;
    }
}

static int rs485_monitor(struct task_pcb *task)
{
    uint32_t index = __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    if (index >= _rx_index)
        return 0;

    uint32_t recv_len = 0;

    __disable_irq();
    do {
        index = __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
        if (index >= _rx_index)
            break;
        if (__HAL_DMA_GET_FLAG(&hdma_usart2_rx, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_usart2_rx)) != RESET)
            break;
        if (__HAL_DMA_GET_FLAG(&hdma_usart2_rx, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_usart2_rx)) != RESET)
            break;

        recv_len = _rx_index - index;
        _rx_index = index;

        if (recv_len > 0) {
            rs485_rx_rb_update(recv_len);
            _rx_tick_timeout = HAL_GetTick() + _byte_timeout;
        }
    } while (0);
    __enable_irq();

    return 0;
}

int rs485_receive(uint8_t *buf, int bufsz, int timeout, int bytes_timeout, int *state)
{
    static int len = 0;
    static int max_len = 0;

    switch (*state) {
    case RS485_RECV_STATE_START: {
        len = 0;
        max_len = bufsz;
        __disable_irq();
        _rx_tick_timeout = HAL_GetTick() + timeout;
        _byte_timeout = bytes_timeout;
        __enable_irq();
        *state = RS485_RECV_STATE_WAIT_FIRST;
    } break;

    case RS485_RECV_STATE_WAIT_FIRST: {
        if (rt_ringbuffer_data_len(&_rx_rb) > 0) {
            *state = RS485_RECV_STATE_WAIT_BYTE;

            __disable_irq();
            _rx_tick_timeout = HAL_GetTick() + _byte_timeout;
            __enable_irq();
        } else {
            if ((HAL_GetTick() - _rx_tick_timeout) < (HAL_TICK_MAX / 2))
                *state = RS485_RECV_STATE_FINISH;
        }
    } break;

    case RS485_RECV_STATE_WAIT_BYTE: {
        if (rt_ringbuffer_data_len(&_rx_rb) > 0) {
            __disable_irq();
            int rc = rt_ringbuffer_get(&_rx_rb, buf + len, max_len);
            __enable_irq();

            len += rc;
            max_len -= rc;

            if (max_len == 0) {
                *state = RS485_RECV_STATE_FINISH;
                break;
            }
        } else {
            if ((HAL_GetTick() - _rx_tick_timeout) < (HAL_TICK_MAX / 2))
                *state = RS485_RECV_STATE_FINISH;
        }
    } break;

    default:
        break;
    }

    return len;
}

int rs485_send(uint8_t *buf, int len, int timeout, int *state)
{
    int send_len = 0;

    switch (*state) {
    case RS485_SEND_STATE_START: {
        *state = RS485_SEND_STATE_WAIT;
        RS485_TX_EN();
        HAL_UART_AbortTransmit(&huart2);
        _tx_complete = 0;
        HAL_UART_Transmit_DMA(&huart2, buf, len);
        _tx_tick_timeout = HAL_GetTick() + timeout;
    } break;

    case RS485_SEND_STATE_WAIT: {
        if (_tx_complete) {
            send_len = len;
            *state = RS485_SEND_STATE_FINISH;
        } else {
            if ((HAL_GetTick() - _tx_tick_timeout) < (HAL_TICK_MAX / 2))
                *state = RS485_SEND_STATE_FINISH;
        }
    } break;

    default: {
        HAL_UART_AbortTransmit(&huart2);
        RS485_RX_EN();
    } break;
    }

    return send_len;
}

void rs485_flush(void)
{
    rt_ringbuffer_init(&_rx_rb, _rx_rb_buf, sizeof(_rx_rb_buf));
    HAL_UART_AbortReceive(&huart2);
    HAL_UART_Receive_DMA(&huart2, _rx_rb.buffer_ptr, _rx_rb.buffer_size);
    _rx_index = _rx_rb.buffer_size;
}

static void modbus_master_process(void)
{
#define PROCESS_INTERVAL 100

    static int __tx_state = RS485_SEND_STATE_START;
    static int __rx_state = RS485_RECV_STATE_START;
    static int __send_len = 0;
    static int __read_len = 0;
    static uint32_t __tick_timeout = 0;

    int *run_step = &_modbus_step[gbl_attr.modbus_mode];
    agile_modbus_t *ctx = &_ctx_rtu._ctx;
    uint16_t hold_register[10];

    switch(*run_step) {
        case 0: {
            agile_modbus_rtu_init(&_ctx_rtu, _ctx_send_buf, sizeof(_ctx_send_buf), _ctx_read_buf1, sizeof(_ctx_read_buf1));
            agile_modbus_set_slave(ctx, 1);
            *run_step = 1;
        } break;

        case 1: {
            __tx_state = RS485_SEND_STATE_START;
            __rx_state = RS485_RECV_STATE_START;
            __send_len = agile_modbus_serialize_read_registers(ctx, 0, 10);
            *run_step = 2;
        } break;

        case 2: {
            int rc = rs485_send(ctx->send_buf, __send_len, 1000, &__tx_state);
            if(__tx_state == RS485_SEND_STATE_FINISH) {
                if(rc != __send_len) {
                    *run_step = 5;
                    LOG_I("send timeout.");
                    __tick_timeout = HAL_GetTick() + PROCESS_INTERVAL;
                }
                else
                    *run_step = 3;
            }
        } break;

        case 3: {
            __read_len = rs485_receive(ctx->read_buf, ctx->read_bufsz, 1000, 50, &__rx_state);
            if(__rx_state == RS485_RECV_STATE_FINISH) {
                if(__read_len == 0) {
                    *run_step = 5;
                    LOG_I("recv timeout.");
                    __tick_timeout = HAL_GetTick() + PROCESS_INTERVAL;
                }
                else
                    *run_step = 4;
            }
        } break;

        case 4: {
            int rc = agile_modbus_deserialize_read_registers(ctx, __read_len, hold_register);
            if (rc < 0) {
                LOG_W("Receive failed.");
            } else {
                LOG_I("Hold Registers:");
                for (int i = 0; i < 10; i++)
                    LOG_I("Register [%d]: 0x%04X", i, hold_register[i]);

                printf("\r\n\r\n\r\n");
            }

            __tick_timeout = HAL_GetTick() + PROCESS_INTERVAL;
            *run_step = 5;
        } break;

        default: {
            if((HAL_GetTick() - __tick_timeout) >= (HAL_TICK_MAX / 2))
                break;

            *run_step = 1;
        }break;
    }
}

static int rs485_modbus(struct task_pcb *task)
{
    if (task->event & RS485_MODBUS_EVENT_SWITCH) {
        task->event &= ~RS485_MODBUS_EVENT_SWITCH;
        gbl_attr.modbus_mode++;
        if (gbl_attr.modbus_mode == MODBUS_MODE_MAX)
            gbl_attr.modbus_mode = MODBUS_MODE_MASTER;

        _modbus_step[gbl_attr.modbus_mode] = 0;

        printf("\r\n\r\n");
        LOG_I("modbus mode switch to %s", modbus_mode_str_tab[gbl_attr.modbus_mode]);
        printf("\r\n\r\n");
    }

    switch (gbl_attr.modbus_mode) {
    case MODBUS_MODE_MASTER:
        modbus_master_process();
        break;

    default:
        break;
    }

    return 0;
}

void rs485_init(void)
{
    rt_ringbuffer_init(&_rx_rb, _rx_rb_buf, sizeof(_rx_rb_buf));
    HAL_UART_AbortReceive(&huart2);
    HAL_UART_Receive_DMA(&huart2, _rx_rb.buffer_ptr, _rx_rb.buffer_size);
    _rx_index = _rx_rb.buffer_size;

    task_init(TASK_INDEX_RS485_MONITOR, rs485_monitor, 10);
    task_start(TASK_INDEX_RS485_MONITOR);

    task_init(TASK_INDEX_RS485_MODBUS, rs485_modbus, 0);
    task_start(TASK_INDEX_RS485_MODBUS);
}
