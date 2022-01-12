#ifndef __COMMON_H
#define __COMMON_H

#define BOOT_BKP       RTC_BKP_DR15
#define BOOT_APP1_FLAG 0x1515
#define BOOT_APP2_FLAG 0x2525

#define BOOT_APP1_ADDR 0x8004000
#define BOOT_APP2_ADDR 0x8020000

#define HAL_UINT32_MAX 0xffffffff     /**< Maxium number of UINT32 */
#define HAL_TICK_MAX   HAL_UINT32_MAX /**< Maxium number of tick */

#endif
