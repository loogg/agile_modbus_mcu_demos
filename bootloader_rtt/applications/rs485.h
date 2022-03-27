#ifndef __RS485_H
#define __RS485_H

#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>

int rs485_init(void);
int rs485_send(uint8_t *buf, int len);
int rs485_receive(uint8_t *buf, int bufsz, int timeout, int bytes_timeout);

#endif
