#ifndef __MB_TCP_H
#define __MB_TCP_H

#include <rtthread.h>
#include <stdint.h>
#include <agile_modbus.h>
#include <agile_modbus_slave_util.h>

#define MBTCP_MAX_CONNECTIONS 3

typedef struct {
    int socket;
    int active;
    int quit;
    int connected;
    uint32_t tick_timeout;
    agile_modbus_tcp_t mb_ctx;
    uint8_t send_buf[260];
    uint8_t read_buf[2048];
} mbtcp_session_t;

typedef struct {
    int socket;
    mbtcp_session_t sessions[MBTCP_MAX_CONNECTIONS];
} mbtcp_server_t;

int mb_tcp_init(void);

#endif /* __MB_TCP_H */
