#ifndef __BOOT_H
#define __BOOT_H

struct boot_state {
    uint8_t app1_enable;
    uint8_t app2_enable;
};
extern struct boot_state boot_en;

void boot_check(void);
void boot_process(void);

#endif
