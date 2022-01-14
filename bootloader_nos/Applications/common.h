#ifndef __COMMON_H
#define __COMMON_H

#include "agile_modbus.h"

/** @defgroup BOOT_DEF  Boot Definition
 * @{
 */
#define BOOT_BKP       RTC_BKP_DR15
#define BOOT_APP1_FLAG 0x1515
#define BOOT_APP2_FLAG 0x2525

#define BOOT_APP1_ADDR 0x8004000
#define BOOT_APP2_ADDR 0x8020000
/**
 * @}
 */

/** @defgroup BASIC_DEF Basic Definition
 * @{
 */
#define HAL_UINT32_MAX 0xffffffff     /**< Maxium number of UINT32 */
#define HAL_TICK_MAX   HAL_UINT32_MAX /**< Maxium number of tick */
/**
 * @}
 */

/** @defgroup RS485_MODBUS_EVENT    RS485 MODBUS EVENT
 * @{
 */
#define RS485_MODBUS_EVENT_SWITCH 0x01
/**
 * @}
 */

/** @defgroup MODBUS_MODE_DEF    MODBUS MODE DEF
 * @{
 */
#define MODBUS_MASTER_ENABLE           1
#define MODBUS_SLAVE_ENABLE            1
#define MODBUS_P2P_UPDATE_ENABLE       1
#define MODBUS_BROADCAST_UPDATE_ENABLE 0

#if MODBUS_BROADCAST_UPDATE_ENABLE
#define MODBUS_RECV_BUF 2048
#else
#define MODBUS_RECV_BUF AGILE_MODBUS_MAX_ADU_LENGTH
#endif
/**
 * @}
 */

enum {
#if MODBUS_MASTER_ENABLE
    MODBUS_MODE_MASTER,
#endif

#if MODBUS_SLAVE_ENABLE
    MODBUS_MODE_SLAVE,
#endif

#if MODBUS_P2P_UPDATE_ENABLE
    MODBUS_MODE_P2P_UPDATE,
#endif

#if MODBUS_BROADCAST_UPDATE_ENABLE
    MODBUS_MODE_BROADCAST_UPDATE,
#endif

    MODBUS_MODE_MAX
};

struct global_attr {
    int modbus_mode;
    agile_modbus_rtu_t ctx_rtu;
    uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
    uint8_t ctx_read_buf1[MODBUS_RECV_BUF];
#if MODBUS_BROADCAST_UPDATE_ENABLE
    uint8_t ctx_read_buf2[MODBUS_RECV_BUF];
#endif
    int modbus_step[MODBUS_MODE_MAX];
};
extern struct global_attr gbl_attr;

extern const char *modbus_mode_str_tab[MODBUS_MODE_MAX];

#endif
