#include "rs485.h"
#include "common.h"
#include "main.h"

#if MODBUS_SLAVE_ENABLE

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "rtu_slave"
#define DBG_LEVEL        DBG_LOG
#include "dbg_log.h"

extern const agile_modbus_slave_util_map_t bit_maps[1];
extern const agile_modbus_slave_util_map_t input_bit_maps[1];
extern const agile_modbus_slave_util_map_t register_maps[1];
extern const agile_modbus_slave_util_map_t input_register_maps[1];

static int addr_check(agile_modbus_t *ctx, struct agile_modbus_slave_info *slave_info)
{
    int slave = slave_info->sft->slave;
    if ((slave != ctx->slave) && (slave != AGILE_MODBUS_BROADCAST_ADDRESS) && (slave != 0xFF))
        return -AGILE_MODBUS_EXCEPTION_UNKNOW;

    return 0;
}

static const agile_modbus_slave_util_t _slave_util = {
    bit_maps,
    sizeof(bit_maps) / sizeof(bit_maps[0]),
    input_bit_maps,
    sizeof(input_bit_maps) / sizeof(input_bit_maps[0]),
    register_maps,
    sizeof(register_maps) / sizeof(register_maps[0]),
    input_register_maps,
    sizeof(input_register_maps) / sizeof(input_register_maps[0]),
    addr_check,
    NULL,
    NULL};

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
        __send_len = agile_modbus_slave_handle(ctx, __read_len, 0, agile_modbus_slave_util_callback, &_slave_util, NULL);
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
