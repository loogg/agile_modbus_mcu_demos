#include "rs485.h"
#include "common.h"
#include "main.h"

#if MODBUS_MASTER_ENABLE

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "rtu_master"
#define DBG_LEVEL        DBG_LOG
#include "dbg_log.h"

void modbus_master_process(void)
{
#define PROCESS_INTERVAL 10

    static int __tx_state = RS485_SEND_STATE_START;
    static int __rx_state = RS485_RECV_STATE_START;
    static int __send_len = 0;
    static int __read_len = 0;
    static uint32_t __tick_timeout = 0;

    int *run_step = &gbl_attr.modbus_step[gbl_attr.modbus_mode];
    agile_modbus_t *ctx = &gbl_attr.ctx_rtu._ctx;
    uint16_t hold_register[10];

    switch (*run_step) {
    case 0: {
        agile_modbus_rtu_init(&gbl_attr.ctx_rtu, gbl_attr.ctx_send_buf, sizeof(gbl_attr.ctx_send_buf),
                              gbl_attr.ctx_read_buf1, sizeof(gbl_attr.ctx_read_buf1));
        agile_modbus_set_slave(ctx, 1);
        *run_step = 1;
    } break;

    case 1: {
        gbl_attr.modbus_total_cnt[gbl_attr.modbus_mode]++;
        __tx_state = RS485_SEND_STATE_START;
        __rx_state = RS485_RECV_STATE_START;
        __send_len = agile_modbus_serialize_read_registers(ctx, 0, 10);
        *run_step = 2;
    } break;

    case 2: {
        int rc = rs485_send(ctx->send_buf, __send_len, 1000, &__tx_state);
        if (__tx_state == RS485_SEND_STATE_FINISH) {
            if (rc != __send_len) {
                *run_step = 5;
                LOG_I("send timeout.");
                __tick_timeout = HAL_GetTick() + PROCESS_INTERVAL;
            } else
                *run_step = 3;
        }
    } break;

    case 3: {
        __read_len = rs485_receive(ctx->read_buf, ctx->read_bufsz, 1000, 50, &__rx_state);
        if (__rx_state == RS485_RECV_STATE_FINISH) {
            if (__read_len == 0) {
                *run_step = 5;
                LOG_I("recv timeout.");
                __tick_timeout = HAL_GetTick() + PROCESS_INTERVAL;
            } else
                *run_step = 4;
        }
    } break;

    case 4: {
        int rc = agile_modbus_deserialize_read_registers(ctx, __read_len, hold_register);
        if (rc < 0) {
            LOG_W("Receive failed.");
        } else {
            gbl_attr.modbus_success_cnt[gbl_attr.modbus_mode]++;
            LOG_I("Hold Registers:");
            for (int i = 0; i < 10; i++)
                LOG_I("Register [%d]: 0x%04X", i, hold_register[i]);

            printf("\r\n\r\n\r\n");
        }

        __tick_timeout = HAL_GetTick() + PROCESS_INTERVAL;
        *run_step = 5;
    } break;

    default: {
        if ((HAL_GetTick() - __tick_timeout) >= (HAL_TICK_MAX / 2))
            break;

        *run_step = 1;
    } break;
    }
}

#endif
