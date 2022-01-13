#ifndef __TASK_RUN_H
#define __TASK_RUN_H
#include <stdint.h>

enum task_index {
    TASK_INDEX_KEY = 0,
    TASK_INDEX_BOOT,
    TASK_INDEX_MASTER,
    TASK_INDEX_SLAVE,
    TASK_INDEX_UPDATE,
    TASK_INDEX_MAX
};

struct task_pcb {
    uint8_t enable;
    uint32_t tick_timeout;
    uint32_t period;
    int (*task_entry)(struct task_pcb *task);
};

void task_init(enum task_index index, int (*task_entry)(struct task_pcb *task), uint32_t period);
void task_start(enum task_index index);
void task_stop(enum task_index index);
void task_process(void);

#endif
