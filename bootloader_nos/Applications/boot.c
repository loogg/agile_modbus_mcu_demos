#include "main.h"
#include "common.h"
#include "boot.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "boot"
#define DBG_LEVEL        DBG_LOG
#include "dbg_log.h"

struct boot_state boot_en = {0};

typedef void (*rt_boot_app_func)(void);

void boot_check(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    __disable_irq();
    RTC_HandleTypeDef RTC_Handler;
    RTC_Handler.Instance = RTC;
    uint32_t bkp_dr15_data = HAL_RTCEx_BKUPRead(&RTC_Handler, BOOT_BKP);
    HAL_RTCEx_BKUPWrite(&RTC_Handler, BOOT_BKP, 0);
    rt_boot_app_func app_func = NULL;
    uint32_t app_addr = 0;
    if (bkp_dr15_data == BOOT_APP1_FLAG)
        app_addr = BOOT_APP1_ADDR;
    else if (bkp_dr15_data == BOOT_APP2_FLAG)
        app_addr = BOOT_APP2_ADDR;

    if (app_addr == 0)
        goto _exit;

    if (((*(__IO uint32_t *)(app_addr + 4)) & 0xff000000) != 0x08000000)
        goto _exit;

    if (((*(__IO uint32_t *)app_addr) & 0x2ffe0000) != 0x20000000)
        goto _exit;

    app_func = (rt_boot_app_func) * (__IO uint32_t *)(app_addr + 4);
    /* Configure main stack */
    __set_MSP(*(__IO uint32_t *)app_addr);
    /* jump to application */
    app_func();

_exit:
    __enable_irq();
}

void boot_process(void)
{
    if (boot_en.app1_enable) {
        boot_en.app1_enable = 0;
        __disable_irq();
        RTC_HandleTypeDef RTC_Handler;
        RTC_Handler.Instance = RTC;
        HAL_RTCEx_BKUPWrite(&RTC_Handler, BOOT_BKP, BOOT_APP1_FLAG);
        HAL_NVIC_SystemReset();
    } else if (boot_en.app2_enable) {
        boot_en.app2_enable = 0;
        __disable_irq();
        RTC_HandleTypeDef RTC_Handler;
        RTC_Handler.Instance = RTC;
        HAL_RTCEx_BKUPWrite(&RTC_Handler, BOOT_BKP, BOOT_APP2_FLAG);
        HAL_NVIC_SystemReset();
    }
}
