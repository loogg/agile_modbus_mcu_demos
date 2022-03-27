/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2017/10/15     bernard      the first version
 */

#ifndef __RTT_LIBC_H__
#define __RTT_LIBC_H__

#include <rtconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

int libc_system_init(void);
#ifdef RT_USING_POSIX_STDIO
int libc_stdio_get_console(void);
int libc_stdio_set_console(const char* device_name, int mode);
#endif /* RT_USING_POSIX_STDIO */

#ifdef __cplusplus
}
#endif

#endif
