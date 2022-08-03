#include "rs485.h"
#include "common.h"
#include "main.h"
#include "drv_flash.h"
#include <string.h>

#if MODBUS_P2P_UPDATE_ENABLE

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "p2p_slave"
#define DBG_LEVEL        DBG_LOG
#include "dbg_log.h"

#define AGILE_MODBUS_FC_TRANS_FILE 0x50
#define TRANS_FILE_CMD_START       0x0001
#define TRANS_FILE_CMD_DATA        0x0002
#define TRANS_FILE_FLAG_END        0x00
#define TRANS_FILE_FLAG_NOT_END    0x01

static uint8_t compute_meta_length_after_function_callback(agile_modbus_t *ctx, int function,
                                                           agile_modbus_msg_type_t msg_type)
{
    int length;

    if (msg_type == AGILE_MODBUS_MSG_INDICATION) {
        length = 0;
        if (function == AGILE_MODBUS_FC_TRANS_FILE)
            length = 4;
    } else {
        /* MSG_CONFIRMATION */
        length = 1;
        if (function == AGILE_MODBUS_FC_TRANS_FILE)
            length = 3;
    }

    return length;
}

static int compute_data_length_after_meta_callback(agile_modbus_t *ctx, uint8_t *msg,
                                                   int msg_length, agile_modbus_msg_type_t msg_type)
{
    int function = msg[ctx->backend->header_length];
    int length;

    if (msg_type == AGILE_MODBUS_MSG_INDICATION) {
        length = 0;
        if (function == AGILE_MODBUS_FC_TRANS_FILE)
            length = (msg[ctx->backend->header_length + 3] << 8) + msg[ctx->backend->header_length + 4];
    } else {
        /* MSG_CONFIRMATION */
        length = 0;
    }

    return length;
}

static void print_progress(size_t cur_size, size_t total_size)
{
    static uint8_t progress_sign[100 + 1];
    uint8_t i, per = cur_size * 100 / total_size;

    if (per > 100) {
        per = 100;
    }

    for (i = 0; i < 100; i++) {
        if (i < per) {
            progress_sign[i] = '=';
        } else if (per == i) {
            progress_sign[i] = '>';
        } else {
            progress_sign[i] = ' ';
        }
    }

    progress_sign[sizeof(progress_sign) - 1] = '\0';

    LOG_I("\033[2A");
    LOG_I("Trans: [%s] %d%%", progress_sign, per);
}

/**
 * @brief   从机回调函数
 * @param   ctx modbus 句柄
 * @param   slave_info 从机信息体
 * @param   data 私有数据
 * @return  =0:正常;
 *          <0:异常
 *             (-AGILE_MODBUS_EXCEPTION_UNKNOW(-255): 未知异常，从机不会打包响应数据)
 *             (其他负数异常码: 从机会打包异常响应数据)
 */
static int slave_callback(agile_modbus_t *ctx, struct agile_modbus_slave_info *slave_info, const void *data)
{
    static int __is_start = 0;
    static uint32_t __file_size = 0;
    static uint32_t __write_file_size = 0;

    int function = slave_info->sft->function;

    if (function != AGILE_MODBUS_FC_TRANS_FILE)
        return 0;

    int ret = 0;

    int send_index = slave_info->send_index;
    uint8_t *data_ptr = slave_info->buf;
    int cmd = (data_ptr[0] << 8) + data_ptr[1];
    int cmd_data_len = (data_ptr[2] << 8) + data_ptr[3];
    uint8_t *cmd_data_ptr = data_ptr + 4;

    switch (cmd) {
    case TRANS_FILE_CMD_START: {
        if (__is_start) {
            LOG_W("update is already start, now abort.");
            __is_start = 0;
        }

        if (cmd_data_len <= 4) {
            LOG_W("cmd start date_len must be greater than 4.");
            ret = -1;
            break;
        }

        __file_size = (((uint32_t)cmd_data_ptr[0] << 24) +
                       ((uint32_t)cmd_data_ptr[1] << 16) +
                       ((uint32_t)cmd_data_ptr[2] << 8) +
                       (uint32_t)cmd_data_ptr[3]);

        __write_file_size = 0;

        char *file_name = (char *)(data_ptr + 8);
        if (strlen(file_name) >= 256) {
            LOG_W("file name must be less than 256.");
            ret = -1;
            break;
        }

        LOG_I("file name is %s, file size is %u", file_name, __file_size);
        printf("\r\n\r\n");

        __is_start = 1;
        gbl_attr.modbus_total_cnt[gbl_attr.modbus_mode]++;
    } break;

    case TRANS_FILE_CMD_DATA: {
        if (__is_start == 0) {
            LOG_W("Haven't received the start command yet.");
            ret = -1;
            break;
        }

        if (cmd_data_len <= 0) {
            LOG_W("cmd data data_len must be greater than 0");
            ret = -1;
            break;
        }

        int flag = cmd_data_ptr[0];
        int file_len = cmd_data_len - 1;
        if (file_len > 0) {
            if (drv_flash_write(BOOT_APP_ADDR + __write_file_size, cmd_data_ptr + 1, file_len) != file_len) {
                LOG_W("write to flash error.");
                ret = -1;
                break;
            }
        }
        __write_file_size += file_len;

        print_progress(__write_file_size, __file_size);

        if (flag == TRANS_FILE_FLAG_END) {
            __is_start = 0;
            printf("\r\n\r\n");
            if (__write_file_size != __file_size) {
                LOG_W("_write_file_size (%u) != _file_size (%u)", __write_file_size, __file_size);
                ret = -1;
                break;
            }

            LOG_I("success.");
            gbl_attr.modbus_success_cnt[gbl_attr.modbus_mode]++;
        }
    } break;

    default:
        ret = -1;
        break;
    }

    ctx->send_buf[send_index++] = data_ptr[0];
    ctx->send_buf[send_index++] = data_ptr[1];
    ctx->send_buf[send_index++] = (ret == 0) ? 0x01 : 0x00;
    *(slave_info->rsp_length) = send_index;

    return 0;
}

void modbus_p2p_process(void)
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
        agile_modbus_set_compute_meta_length_after_function_cb(ctx, compute_meta_length_after_function_callback);
        agile_modbus_set_compute_data_length_after_meta_cb(ctx, compute_data_length_after_meta_callback);

        if (gbl_attr.erase_flag) {
            gbl_attr.erase_flag = 0;
            printf("\r\n\r\n");
            LOG_I("Eraseing app flash...");
            if (drv_flash_erase(BOOT_APP_ADDR, BOOT_APP_SIZE) != BOOT_APP_SIZE)
                LOG_W("Erase app flash failed.");
            else
                LOG_I("Erase app flash success.");
        }

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
        __send_len = agile_modbus_slave_handle(ctx, __read_len, 1, slave_callback, NULL, NULL);
        if (__send_len <= 0)
            *run_step = 1;
        else
            *run_step = 4;
    } break;

    default: {
        int rc = rs485_send(ctx->send_buf, __send_len, 1000, &__tx_state);
        if (__tx_state == RS485_SEND_STATE_FINISH)
            *run_step = 1;
    } break;
    }
}

#endif
