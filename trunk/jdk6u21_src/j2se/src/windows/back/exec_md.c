/*
 * @(#)exec_md.c	1.11 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include <windows.h>
#include <string.h>
#include "sys.h"
  

int 
dbgsysExec(char *cmdLine)
{
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    int ret;

    if (cmdLine == 0) {
        return SYS_ERR;
    }

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    ret = CreateProcess(0,                /* executable name */
                        cmdLine,          /* command line */
                        0,                /* process security attribute */
                        0,                /* thread security attribute */
                        TRUE,             /* inherits system handles */
                        0,                /* normal attached process */
                        0,                /* environment block */
                        0,                /* inherits the current directory */
                        &si,              /* (in)  startup information */
                        &pi);             /* (out) process information */

    if (ret == 0) {
        return SYS_ERR;
    } else {
        return SYS_OK;
    }
}

