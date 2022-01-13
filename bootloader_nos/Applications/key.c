#include <string.h>
#include "main.h"
#include "common.h"
#include "task_run.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "key"
#define DBG_LEVEL        DBG_LOG
#include "dbg_log.h"

/**
 * @brief   Agile Button 对象事件
 */
enum agile_btn_event {
    BTN_PRESS_DOWN_EVENT = 0, /**< 按下事件 */
    BTN_HOLD_EVENT,           /**< 持续按下有效事件 */
    BTN_PRESS_UP_EVENT,       /**< 弹起事件 */
    BTN_CLICK_EVENT,          /**< 点击事件 */
    BTN_EVENT_SUM             /**< 事件总数目 */
};

/**
 * @brief   Agile Button 对象状态
 */
enum agile_btn_state {
    BTN_STATE_NONE_PRESS = 0, /**< 未按下状态 */
    BTN_STATE_CHECK_PRESS,    /**< 抖动检查状态 */
    BTN_STATE_PRESS_DOWN,     /**< 按下状态 */
    BTN_STATE_PRESS_HOLD,     /**< 持续按下状态 */
    BTN_STATE_PRESS_UP,       /**< 弹起状态 */
};

typedef struct agile_btn agile_btn_t; /**< Agile Button 结构体 */

/**
 * @brief   Agile Button 结构体
 */
struct agile_btn {
    GPIO_TypeDef *gpio;                                /**< GPIO */
    uint32_t pin;                                      /**< PIN */
    uint8_t repeat_cnt;                                /**< 按键重按计数 */
    enum agile_btn_event event;                        /**< 按键对象事件 */
    enum agile_btn_state state;                        /**< 按键对象状态 */
    uint32_t hold_time;                                /**< 按键按下持续时间(单位ms) */
    uint32_t prev_hold_time;                           /**< 缓存 hold_time 变量 */
    uint32_t active_logic;                             /**< 有效电平 */
    uint32_t tick_timeout;                             /**< 超时时间 */
    void (*event_cb[BTN_EVENT_SUM])(agile_btn_t *btn); /**< 按键对象事件回调函数 */
};

#define AGILE_BUTTON_ELIMINATION_TIME_DEFAULT  15   /**< 按键消抖默认时间 15ms */
#define AGILE_BUTTON_TWO_INTERVAL_TIME_DEFAULT 200  /**< 两次按键按下间隔超过200ms清零重复计数 */
#define AGILE_BUTTON_HOLD_CYCLE_TIME_DEFAULT   1000 /**< 按键按下后持续调用回调函数的周期 */

/**
 * @brief   获取按键引脚电平状态
 * @param   btn Agile Button 对象指针
 * @return  引脚电平
 */
#define AGILE_BUTTON_PIN_STATE(btn) (int)HAL_GPIO_ReadPin(btn->gpio, btn->pin)

/**
 * @brief   调用按键事件的回调函数
 * @param   btn Agile Button 对象指针
 * @param   event 事件类型
 * @return  无
 */
#define AGILE_BUTTON_EVENT_CB(btn, event) \
    do {                                  \
        if (btn->event_cb[event]) {       \
            btn->event_cb[event](btn);    \
        }                                 \
    } while (0)

/**
 * @brief   计算按键按下持续时间
 * @param   btn Agile Button 对象指针
 */
static void agile_btn_cal_hold_time(agile_btn_t *btn)
{
    if (HAL_GetTick() < btn->tick_timeout) {
        btn->hold_time = HAL_TICK_MAX - btn->tick_timeout + HAL_GetTick();
    } else {
        btn->hold_time = HAL_GetTick() - btn->tick_timeout;
    }
}

static int agile_btn_init(agile_btn_t *btn, GPIO_TypeDef *gpio, uint32_t pin, uint32_t active_logic)
{
    memset(btn, 0, sizeof(agile_btn_t));

    btn->gpio = gpio;
    btn->pin = pin;
    btn->repeat_cnt = 0;
    btn->event = BTN_EVENT_SUM;
    btn->state = BTN_STATE_NONE_PRESS;
    btn->hold_time = 0;
    btn->prev_hold_time = 0;
    btn->active_logic = active_logic;
    btn->tick_timeout = HAL_GetTick();

    return 0;
}

static int agile_btn_set_event_cb(agile_btn_t *btn, enum agile_btn_event event, void (*event_cb)(agile_btn_t *btn))
{
    if (event >= BTN_EVENT_SUM)
        return -1;

    btn->event_cb[event] = event_cb;

    return 0;
}

#define TASK_RUN_PERIOD 5

static agile_btn_t _btn_tab[3] = {0};

static int task_entry(struct task_pcb *task)
{
    for (int i = 0; i < sizeof(_btn_tab) / sizeof(_btn_tab[0]); i++) {
        agile_btn_t *btn = &_btn_tab[i];

        switch (btn->state) {
        case BTN_STATE_NONE_PRESS: {
            if (AGILE_BUTTON_PIN_STATE(btn) == btn->active_logic) {
                btn->tick_timeout = HAL_GetTick() + AGILE_BUTTON_ELIMINATION_TIME_DEFAULT;
                btn->state = BTN_STATE_CHECK_PRESS;
            } else {
                /* 2次按下中间间隔过大，清零重按计数 */
                if (btn->repeat_cnt) {
                    if ((HAL_GetTick() - btn->tick_timeout) < (HAL_TICK_MAX / 2)) {
                        btn->repeat_cnt = 0;
                    }
                }
            }
        } break;
        case BTN_STATE_CHECK_PRESS: {
            if (AGILE_BUTTON_PIN_STATE(btn) == btn->active_logic) {
                if ((HAL_GetTick() - btn->tick_timeout) < (HAL_TICK_MAX / 2)) {
                    btn->state = BTN_STATE_PRESS_DOWN;
                }
            } else {
                btn->state = BTN_STATE_NONE_PRESS;
            }
        } break;
        case BTN_STATE_PRESS_DOWN: {
            btn->hold_time = 0;
            btn->prev_hold_time = 0;
            btn->repeat_cnt++;
            btn->event = BTN_PRESS_DOWN_EVENT;
            AGILE_BUTTON_EVENT_CB(btn, btn->event);

            btn->tick_timeout = HAL_GetTick();
            btn->state = BTN_STATE_PRESS_HOLD;
        } break;
        case BTN_STATE_PRESS_HOLD: {
            if (AGILE_BUTTON_PIN_STATE(btn) == btn->active_logic) {
                agile_btn_cal_hold_time(btn);
                if (btn->hold_time - btn->prev_hold_time >= AGILE_BUTTON_HOLD_CYCLE_TIME_DEFAULT) {
                    btn->event = BTN_HOLD_EVENT;
                    AGILE_BUTTON_EVENT_CB(btn, btn->event);
                    btn->prev_hold_time = btn->hold_time;
                }
            } else {
                btn->state = BTN_STATE_PRESS_UP;
            }
        } break;
        case BTN_STATE_PRESS_UP: {
            btn->event = BTN_PRESS_UP_EVENT;
            AGILE_BUTTON_EVENT_CB(btn, btn->event);
            btn->event = BTN_CLICK_EVENT;
            AGILE_BUTTON_EVENT_CB(btn, btn->event);

            btn->tick_timeout = HAL_GetTick() + AGILE_BUTTON_TWO_INTERVAL_TIME_DEFAULT;
            btn->state = BTN_STATE_NONE_PRESS;
        } break;
        default:
            break;
        }
    }

    return 0;
}

static void key0_click_cb(agile_btn_t *btn)
{
    LOG_I("key0. %d", btn->repeat_cnt);
}

static void key1_click_cb(agile_btn_t *btn)
{
    task_event_send(TASK_INDEX_RS485_MODBUS, RS485_MODBUS_EVENT_SWITCH);
}

extern void print_help(void);
static void key2_click_cb(agile_btn_t *btn)
{
    print_help();
}

void key_init(void)
{
    agile_btn_init(&_btn_tab[0], KEY0_GPIO_Port, KEY0_Pin, 0);
    agile_btn_set_event_cb(&_btn_tab[0], BTN_CLICK_EVENT, key0_click_cb);

    agile_btn_init(&_btn_tab[1], KEY1_GPIO_Port, KEY1_Pin, 0);
    agile_btn_set_event_cb(&_btn_tab[1], BTN_CLICK_EVENT, key1_click_cb);

    agile_btn_init(&_btn_tab[2], KEY2_GPIO_Port, KEY2_Pin, 0);
    agile_btn_set_event_cb(&_btn_tab[2], BTN_CLICK_EVENT, key2_click_cb);


    task_init(TASK_INDEX_KEY, task_entry, TASK_RUN_PERIOD);
    task_start(TASK_INDEX_KEY);
}
