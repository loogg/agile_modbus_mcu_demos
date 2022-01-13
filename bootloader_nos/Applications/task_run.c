#include "task_run.h"
#include "main.h"
#include "common.h"
#include <string.h>

static struct task_pcb _task_tab[TASK_INDEX_MAX] = {0};

void task_init(enum task_index index, int (*task_entry)(struct task_pcb *task), uint32_t period)
{
    memset(&_task_tab[index], 0, sizeof(struct task_pcb));

    _task_tab[index].tick_timeout = 0;
    _task_tab[index].period = period;
    _task_tab[index].task_entry = task_entry;
}

void task_start(enum task_index index)
{
    _task_tab[index].enable = 1;
}

void task_stop(enum task_index index)
{
    _task_tab[index].enable = 0;
}

int task_event_send(enum task_index index, uint32_t set)
{
    if(_task_tab[index].enable == 0)
        return -1;

    _task_tab[index].event |= set;

    return 0;
}

void task_process(void)
{
    for (int i = 0; i < sizeof(_task_tab) / sizeof(_task_tab[0]); i++) {
        struct task_pcb *task = &_task_tab[i];
        if ((task->enable == 0) || (task->task_entry == NULL))
            continue;

        if ((HAL_GetTick() - task->tick_timeout) >= (HAL_TICK_MAX / 2))
            continue;

        if (task->task_entry)
            task->task_entry(task);

        task->tick_timeout = HAL_GetTick() + task->period;
    }
}
