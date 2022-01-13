#ifndef __COMMON_H
#define __COMMON_H

#define BOOT_BKP       RTC_BKP_DR15
#define BOOT_APP1_FLAG 0x1515
#define BOOT_APP2_FLAG 0x2525

#define BOOT_APP1_ADDR 0x8004000
#define BOOT_APP2_ADDR 0x8020000

#define HAL_UINT32_MAX 0xffffffff     /**< Maxium number of UINT32 */
#define HAL_TICK_MAX   HAL_UINT32_MAX /**< Maxium number of tick */

#define RS485_MODBUS_EVENT_SWITCH   0x01

enum
{
    MODBUS_MODE_MASTER = 0,
    MODBUS_MODE_SLAVE,
    MODBUS_MODE_P2P_UPDATE,
    MODBUS_MODE_BROADCAST_UPDATE,
    MODBUS_MODE_MAX
};

struct global_attr {
    int modbus_mode;
};
extern struct global_attr gbl_attr;

extern const char *modbus_mode_str_tab[MODBUS_MODE_MAX];

#endif
