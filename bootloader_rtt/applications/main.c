#include <rtthread.h>
#include <rtdevice.h>
#include "rs485.h"
#include <agile_modbus.h>

#define DBG_TAG "rtu_master"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

int main(void)
{
    rs485_init();

    uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
    uint8_t ctx_read_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
    uint16_t hold_register[10];

    agile_modbus_rtu_t ctx_rtu;
    agile_modbus_t *ctx = &ctx_rtu._ctx;
    agile_modbus_rtu_init(&ctx_rtu, ctx_send_buf, sizeof(ctx_send_buf), ctx_read_buf, sizeof(ctx_read_buf));
    agile_modbus_set_slave(ctx, 1);

    LOG_I("Running.");

    while (1)
    {
        rt_thread_mdelay(100);

        int send_len = agile_modbus_serialize_read_registers(ctx, 0, 10);
        rs485_send(ctx->send_buf, send_len);
        int read_len = rs485_receive(ctx->read_buf, ctx->read_bufsz, 1000, 20);

        if (read_len == 0)
        {
            LOG_W("Receive timeout.");
            continue;
        }

        int rc = agile_modbus_deserialize_read_registers(ctx, read_len, hold_register);
        if (rc < 0)
        {
            LOG_W("Receive failed.");
            if (rc != -1)
                LOG_W("Error code:%d", -128 - rc);

            continue;
        }

        LOG_I("Hold Registers:");
        for (int i = 0; i < 10; i++)
            LOG_I("Register [%d]: 0x%04X", i, hold_register[i]);

        rt_kprintf("\r\n\r\n\r\n");
    }
}
