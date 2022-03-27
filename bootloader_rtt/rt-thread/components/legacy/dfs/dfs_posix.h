/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-05-27     Yi.qiu       The first version.
 * 2010-07-18     Bernard      add stat and statfs structure definitions.
 * 2011-05-16     Yi.qiu       Change parameter name of rename, "new" is C++ key word.
 * 2017-12-27     Bernard      Add fcntl API.
 * 2018-02-07     Bernard      Change the 3rd parameter of open/fcntl/ioctl to '...'
 */

#ifndef __DFS_POSIX_H__
#define __DFS_POSIX_H__

#include <dfs_file.h>
#include <unistd.h>
#include <stdio.h> /* rename() */
#include <sys/stat.h>
#include <sys/statfs.h> /* statfs() */

#endif
