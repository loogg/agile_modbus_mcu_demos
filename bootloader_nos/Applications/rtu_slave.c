#include "rs485.h"
#include "common.h"
#include "main.h"

#if MODBUS_SLAVE_ENABLE

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "rtu_slave"
#define DBG_LEVEL        DBG_LOG
#include "dbg_log.h"

#define TAB_MAX_NUM 10
static uint8_t _tab_bits[TAB_MAX_NUM] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
static uint8_t _tab_input_bits[TAB_MAX_NUM] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
static uint16_t _tab_registers[TAB_MAX_NUM] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static uint16_t _tab_input_registers[TAB_MAX_NUM] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

/**
 * @brief   从机回调函数
 * @param   ctx modbus 句柄
 * @param   slave_info 从机信息体
 * @return  =0:正常;
 *          <0:异常
 *             (-AGILE_MODBUS_EXCEPTION_UNKNOW(-255): 未知异常，从机不会打包响应数据)
 *             (其他负数异常码: 从机会打包异常响应数据)
 */
static int slave_callback(agile_modbus_t *ctx, struct agile_modbus_slave_info *slave_info)
{
    int function = slave_info->sft->function;
    int ret = 0;

    switch (function) {
    case AGILE_MODBUS_FC_READ_COILS:
    case AGILE_MODBUS_FC_READ_DISCRETE_INPUTS: {
        int address = slave_info->address;
        int nb = slave_info->nb;
        int send_index = slave_info->send_index;
        int is_input = (function == AGILE_MODBUS_FC_READ_DISCRETE_INPUTS);

        for (int now_address = address, i = 0; now_address < address + nb; now_address++, i++) {
            if (now_address >= 0 && now_address < TAB_MAX_NUM) {
                int index = now_address - 0;
                agile_modbus_slave_io_set(ctx->send_buf + send_index, i,
                                          is_input ? _tab_input_bits[index] : _tab_bits[index]);
            }
        }
    } break;

    case AGILE_MODBUS_FC_READ_HOLDING_REGISTERS:
    case AGILE_MODBUS_FC_READ_INPUT_REGISTERS: {
        int address = slave_info->address;
        int nb = slave_info->nb;
        int send_index = slave_info->send_index;
        int is_input = (function == AGILE_MODBUS_FC_READ_INPUT_REGISTERS);

        for (int now_address = address, i = 0; now_address < address + nb; now_address++, i++) {
            if (now_address >= 0 && now_address < TAB_MAX_NUM) {
                int index = now_address - 0;
                agile_modbus_slave_register_set(ctx->send_buf + send_index, i,
                                                is_input ? _tab_input_registers[index] : _tab_registers[index]);
            }
        }
    } break;

    case AGILE_MODBUS_FC_WRITE_SINGLE_COIL:
    case AGILE_MODBUS_FC_WRITE_MULTIPLE_COILS: {
        int address = slave_info->address;

        if (function == AGILE_MODBUS_FC_WRITE_SINGLE_COIL) {
            if (address >= 0 && address < TAB_MAX_NUM) {
                int index = address - 0;
                int data = *((int *)slave_info->buf);
                _tab_bits[index] = data;
            }
        } else {
            int nb = slave_info->nb;
            for (int now_address = address, i = 0; now_address < address + nb; now_address++, i++) {
                if (now_address >= 0 && now_address < TAB_MAX_NUM) {
                    int index = now_address - 0;
                    uint8_t status = agile_modbus_slave_io_get(slave_info->buf, i);
                    _tab_bits[index] = status;
                }
            }
        }
    } break;

    case AGILE_MODBUS_FC_WRITE_SINGLE_REGISTER:
    case AGILE_MODBUS_FC_WRITE_MULTIPLE_REGISTERS: {
        int address = slave_info->address;

        if (function == AGILE_MODBUS_FC_WRITE_SINGLE_REGISTER) {
            if (address >= 0 && address < TAB_MAX_NUM) {
                int index = address - 0;
                int data = *((int *)slave_info->buf);
                _tab_registers[index] = data;
            }
        } else {
            int nb = slave_info->nb;
            for (int now_address = address, i = 0; now_address < address + nb; now_address++, i++) {
                if (now_address >= 0 && now_address < TAB_MAX_NUM) {
                    int index = now_address - 0;
                    uint16_t data = agile_modbus_slave_register_get(slave_info->buf, i);
                    _tab_registers[index] = data;
                }
            }
        }

    } break;

    case AGILE_MODBUS_FC_MASK_WRITE_REGISTER: {
        int address = slave_info->address;
        if (address >= 0 && address < TAB_MAX_NUM) {
            int index = address - 0;
            uint16_t data = _tab_registers[index];
            uint16_t and = (slave_info->buf[0] << 8) + slave_info->buf[1];
            uint16_t or = (slave_info->buf[2] << 8) + slave_info->buf[3];

            data = (data & and) | (or &(~and));

            _tab_registers[index] = data;
        }
    } break;

    case AGILE_MODBUS_FC_WRITE_AND_READ_REGISTERS: {
        int address = slave_info->address;
        int nb = (slave_info->buf[0] << 8) + slave_info->buf[1];
        uint16_t address_write = (slave_info->buf[2] << 8) + slave_info->buf[3];
        int nb_write = (slave_info->buf[4] << 8) + slave_info->buf[5];
        int send_index = slave_info->send_index;

        /* Write first. 7 is the offset of the first values to write */
        for (int now_address = address_write, i = 0; now_address < address_write + nb_write; now_address++, i++) {
            if (now_address >= 0 && now_address < TAB_MAX_NUM) {
                int index = now_address - 0;
                uint16_t data = agile_modbus_slave_register_get(slave_info->buf + 7, i);
                _tab_registers[index] = data;
            }
        }

        /* and read the data for the response */
        for (int now_address = address, i = 0; now_address < address + nb; now_address++, i++) {
            if (now_address >= 0 && now_address < TAB_MAX_NUM) {
                int index = now_address - 0;
                agile_modbus_slave_register_set(ctx->send_buf + send_index, i, _tab_registers[index]);
            }
        }
    } break;

    default:
        ret = -AGILE_MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
        break;
    }

    return ret;
}

void modbus_slave_process(void)
{
    static int __tx_state = RS485_SEND_STATE_START;
    static int __rx_state = RS485_RECV_STATE_START;
    static int __send_len = 0;
    static int __read_len = 0;

    int *run_step = &gbl_attr.modbus_step[gbl_attr.modbus_mode];
    agile_modbus_t *ctx = &gbl_attr.ctx_rtu._ctx;

    switch (*run_step) {
    case 0: {
        agile_modbus_rtu_init(&gbl_attr.ctx_rtu, gbl_attr.ctx_send_buf, sizeof(gbl_attr.ctx_send_buf),
                              gbl_attr.ctx_read_buf1, sizeof(gbl_attr.ctx_read_buf1));
        agile_modbus_set_slave(ctx, 1);
        *run_step = 1;
    } break;

    case 1: {
        __tx_state = RS485_SEND_STATE_START;
        __rx_state = RS485_RECV_STATE_START;
        *run_step = 2;
    } break;

    case 2: {
        __read_len = rs485_receive(ctx->read_buf, ctx->read_bufsz, 1000, 50, &__rx_state);
        if (__rx_state == RS485_RECV_STATE_FINISH) {
            if (__read_len == 0)
                *run_step = 1;
            else
                *run_step = 3;
        }
    } break;

    case 3: {
        gbl_attr.modbus_total_cnt[gbl_attr.modbus_mode]++;
        __send_len = agile_modbus_slave_handle(ctx, __read_len, 1, slave_callback, NULL);
        if (__send_len <= 0)
            *run_step = 1;
        else
            *run_step = 4;
    } break;

    default: {
        int rc = rs485_send(ctx->send_buf, __send_len, 1000, &__tx_state);
        if (__tx_state == RS485_SEND_STATE_FINISH) {
            if (rc == __send_len)
                gbl_attr.modbus_success_cnt[gbl_attr.modbus_mode]++;

            *run_step = 1;
        }
    } break;
    }
}

#endif
