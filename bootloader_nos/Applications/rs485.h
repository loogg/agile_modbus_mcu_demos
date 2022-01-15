#ifndef __RS485_H
#define __RS485_H

#include <stdint.h>

enum {
    RS485_RECV_STATE_START = 0,
    RS485_RECV_STATE_WAIT_FIRST,
    RS485_RECV_STATE_WAIT_BYTE,
    RS485_RECV_STATE_FINISH
};

enum {
    RS485_SEND_STATE_START = 0,
    RS485_SEND_STATE_WAIT,
    RS485_SEND_STATE_FINISH
};

void rs485_flush(void);
void rs485_init(void);
int rs485_receive(uint8_t *buf, int bufsz, int timeout, int bytes_timeout, int *state);
int rs485_send(uint8_t *buf, int len, int timeout, int *state);

#endif
