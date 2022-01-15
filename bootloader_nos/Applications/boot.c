#include "main.h"
#include "common.h"
#include "boot.h"
#include "task_run.h"

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
    uint32_t bkp_data = HAL_RTCEx_BKUPRead(&RTC_Handler, BOOT_BKP);
    HAL_RTCEx_BKUPWrite(&RTC_Handler, BOOT_BKP, 0);
    if (bkp_data == BOOT_APP_FLAG) {
        rt_boot_app_func app_func = NULL;
        uint32_t app_addr = BOOT_APP_ADDR;

        if (((*(__IO uint32_t *)(app_addr + 4)) & 0xff000000) != 0x08000000)
            goto _exit;

        if (((*(__IO uint32_t *)app_addr) & 0x2ffe0000) != 0x20000000)
            goto _exit;

        app_func = (rt_boot_app_func) * (__IO uint32_t *)(app_addr + 4);
        /* Configure main stack */
        __set_MSP(*(__IO uint32_t *)app_addr);
        /* jump to application */
        app_func();
    }

_exit:
    __enable_irq();
}

static int boot_entry(struct task_pcb *task)
{
    if (task->event & BOOT_EVENT_RUN_APP) {
        task->event &= ~BOOT_EVENT_RUN_APP;

        __disable_irq();
        RTC_HandleTypeDef RTC_Handler;
        RTC_Handler.Instance = RTC;
        HAL_RTCEx_BKUPWrite(&RTC_Handler, BOOT_BKP, BOOT_APP_FLAG);
        HAL_NVIC_SystemReset();
    }

    return 0;
}

void boot_init(void)
{
    task_init(TASK_INDEX_BOOT, boot_entry, 0);
    task_start(TASK_INDEX_BOOT);
}
