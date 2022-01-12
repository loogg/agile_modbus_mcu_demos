#include "main.h"
#include "common.h"
#include "boot.h"
#include <string.h>

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "usr_key"
#define DBG_LEVEL        DBG_LOG
#include "dbg_log.h"

enum usr_key_event {
    KEY_PRESS_DOWN_EVENT = 0, /**< 按下事件 */
    KEY_PRESS_UP_EVENT,       /**< 弹起事件 */
    KEY_CLICK_EVENT,          /**< 点击事件 */
    KEY_EVENT_SUM             /**< 事件总数目 */
};

enum usr_key_state {
    KEY_STATE_NONE_PRESS = 0, /**< 未按下状态 */
    KEY_STATE_CHECK_PRESS,    /**< 抖动检查状态 */
    KEY_STATE_PRESS_DOWN,     /**< 按下状态 */
    KEY_STATE_PRESS_HOLD,     /**< 持续按下状态 */
    KEY_STATE_PRESS_UP,       /**< 弹起状态 */
};

struct usr_key {
    GPIO_TypeDef *gpio;
    uint32_t pin;
    enum usr_key_state state;
    enum usr_key_event event;
    void (*event_cb[KEY_EVENT_SUM])(struct usr_key *key);
    uint32_t tick_timeout;
    int active_logic;
};

#define USR_KEY_ELIMINATION_TIME 15
#define TASK_RUN_PERIOD          5

#define USR_KEY_EVENT_CB(key, event)   \
    do {                               \
        if (key->event_cb[event]) {    \
            key->event_cb[event](key); \
        }                              \
    } while (0)

static struct usr_key _key_tab[2] = {0};

static void key0_click_cb(struct usr_key *key)
{
    LOG_I("will run app1.");
    boot_en.app1_enable = 1;
}

static void key1_click_cb(struct usr_key *key)
{
    LOG_I("will run app2.");
    boot_en.app2_enable = 1;
}

void usr_key_init(void)
{
    _key_tab[0].gpio = KEY0_GPIO_Port;
    _key_tab[0].pin = KEY0_Pin;
    _key_tab[0].state = KEY_STATE_NONE_PRESS;
    _key_tab[0].event_cb[KEY_CLICK_EVENT] = key0_click_cb;
    _key_tab[0].tick_timeout = HAL_GetTick();
    _key_tab[0].active_logic = 0;

    _key_tab[1].gpio = KEY1_GPIO_Port;
    _key_tab[1].pin = KEY1_Pin;
    _key_tab[1].state = KEY_STATE_NONE_PRESS;
    _key_tab[1].event_cb[KEY_CLICK_EVENT] = key1_click_cb;
    _key_tab[1].tick_timeout = HAL_GetTick();
    _key_tab[1].active_logic = 0;
}

void usr_key_process(void)
{
    static uint32_t task_timeout = 0;

    if ((HAL_GetTick() - task_timeout) >= (HAL_TICK_MAX / 2))
        return;

    for (int i = 0; i < sizeof(_key_tab) / sizeof(_key_tab[0]); i++) {
        struct usr_key *key = &_key_tab[i];

        switch (key->state) {
        case KEY_STATE_NONE_PRESS: {
            if ((int)HAL_GPIO_ReadPin(key->gpio, key->pin) == key->active_logic) {
                key->tick_timeout = HAL_GetTick() + USR_KEY_ELIMINATION_TIME;
                key->state = KEY_STATE_CHECK_PRESS;
            }
        } break;
        case KEY_STATE_CHECK_PRESS: {
            if ((int)HAL_GPIO_ReadPin(key->gpio, key->pin) == key->active_logic) {
                if ((HAL_GetTick() - key->tick_timeout) < (HAL_TICK_MAX / 2)) {
                    key->state = KEY_STATE_PRESS_DOWN;
                }
            } else {
                key->state = KEY_STATE_NONE_PRESS;
            }
        } break;
        case KEY_STATE_PRESS_DOWN: {
            key->event = KEY_PRESS_DOWN_EVENT;
            USR_KEY_EVENT_CB(key, key->event);

            key->tick_timeout = HAL_GetTick();
            key->state = KEY_STATE_PRESS_HOLD;
        } break;
        case KEY_STATE_PRESS_HOLD: {
            if ((int)HAL_GPIO_ReadPin(key->gpio, key->pin) != key->active_logic) {
                key->state = KEY_STATE_PRESS_UP;
            }
        } break;
        case KEY_STATE_PRESS_UP: {
            key->event = KEY_PRESS_UP_EVENT;
            USR_KEY_EVENT_CB(key, key->event);
            key->event = KEY_CLICK_EVENT;
            USR_KEY_EVENT_CB(key, key->event);

            key->state = KEY_STATE_NONE_PRESS;
        } break;
        default:
            break;
        }
    }

    task_timeout = HAL_GetTick() + TASK_RUN_PERIOD;
}
