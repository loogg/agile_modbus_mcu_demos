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

#if MODBUS_BROADCAST_UPDATE_ENABLE
#define RS485_RX_RB_BUFSZ 10240
#elif MODBUS_P2P_UPDATE_ENABLE
#define RS485_RX_RB_BUFSZ 2048
#else
#define RS485_RX_RB_BUFSZ 1024
#endif

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;

static struct rt_ringbuffer _rx_rb = {0};
static uint8_t _rx_rb_buf[RS485_RX_RB_BUFSZ];
static uint32_t _rx_index = 0;
static uint32_t _rx_tick_timeout = 0;
static uint32_t _byte_timeout = 0;
static uint32_t _tx_complete = 0;

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
        if (rs485_rx_rb_update(recv_len) < 0)
            rs485_flush();
        else
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
        if (rs485_rx_rb_update(recv_len) < 0)
            rs485_flush();
        else
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
            if (rs485_rx_rb_update(recv_len) < 0)
                rs485_flush();
            else
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
    static uint32_t __tx_tick_timeout = 0;
    int send_len = 0;

    switch (*state) {
    case RS485_SEND_STATE_START: {
        *state = RS485_SEND_STATE_WAIT;
        RS485_TX_EN();
        HAL_UART_AbortTransmit(&huart2);
        _tx_complete = 0;
        HAL_UART_Transmit_DMA(&huart2, buf, len);
        __tx_tick_timeout = HAL_GetTick() + timeout;
    } break;

    case RS485_SEND_STATE_WAIT: {
        if (_tx_complete) {
            send_len = len;
            *state = RS485_SEND_STATE_FINISH;
        } else {
            if ((HAL_GetTick() - __tx_tick_timeout) < (HAL_TICK_MAX / 2))
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

#if MODBUS_MASTER_ENABLE
extern void modbus_master_process(void);
#endif

#if MODBUS_SLAVE_ENABLE
extern void modbus_slave_process(void);
#endif

#if MODBUS_P2P_UPDATE_ENABLE
extern void modbus_p2p_process(void);
#endif

#if MODBUS_BROADCAST_UPDATE_ENABLE
extern void modbus_broadcast_process(void);
#endif

static int rs485_modbus(struct task_pcb *task)
{
    if (task->event & RS485_MODBUS_EVENT_SWITCH) {
        task->event &= ~RS485_MODBUS_EVENT_SWITCH;
        gbl_attr.modbus_mode++;
        if (gbl_attr.modbus_mode == MODBUS_MODE_MAX)
            gbl_attr.modbus_mode = 0;

        gbl_attr.erase_flag = 0;
        gbl_attr.modbus_step[gbl_attr.modbus_mode] = 0;
        gbl_attr.modbus_total_cnt[gbl_attr.modbus_mode] = 0;
        gbl_attr.modbus_success_cnt[gbl_attr.modbus_mode] = 0;

        printf("\r\n\r\n");
        LOG_I("modbus mode switch to %s", modbus_mode_str_tab[gbl_attr.modbus_mode]);
        printf("\r\n\r\n");
    }

    if (task->event & RS485_MODBUS_UPDATE_ERASE) {
        task->event &= ~RS485_MODBUS_UPDATE_ERASE;

#if MODBUS_P2P_UPDATE_ENABLE
        if (gbl_attr.modbus_mode == MODBUS_MODE_P2P_UPDATE) {
            gbl_attr.modbus_step[gbl_attr.modbus_mode] = 0;
            gbl_attr.erase_flag = 1;
        }
#endif

#if MODBUS_BROADCAST_UPDATE_ENABLE
        if (gbl_attr.modbus_mode == MODBUS_MODE_BROADCAST_UPDATE) {
            gbl_attr.modbus_step[gbl_attr.modbus_mode] = 0;
            gbl_attr.erase_flag = 1;
        }
#endif
    }

    switch (gbl_attr.modbus_mode) {
#if MODBUS_MASTER_ENABLE
    case MODBUS_MODE_MASTER:
        modbus_master_process();
        break;
#endif

#if MODBUS_SLAVE_ENABLE
    case MODBUS_MODE_SLAVE:
        modbus_slave_process();
        break;
#endif

#if MODBUS_P2P_UPDATE_ENABLE
    case MODBUS_MODE_P2P_UPDATE:
        modbus_p2p_process();
        break;
#endif

#if MODBUS_BROADCAST_UPDATE_ENABLE
    case MODBUS_MODE_BROADCAST_UPDATE:
        modbus_broadcast_process();
        break;
#endif

    default:
        break;
    }

    return 0;
}

void rs485_flush(void)
{
    rt_ringbuffer_init(&_rx_rb, _rx_rb_buf, sizeof(_rx_rb_buf));
    HAL_UART_AbortReceive(&huart2);
    HAL_UART_Receive_DMA(&huart2, _rx_rb.buffer_ptr, _rx_rb.buffer_size);
    _rx_index = _rx_rb.buffer_size;
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
