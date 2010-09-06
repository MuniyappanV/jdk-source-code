#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)jvm_solaris.h	1.18 03/12/23 16:37:43 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

/*
// HotSpot integration note:
//
// This is derived from the JDK classic file:
// "$JDK/src/solaris/javavm/export/jvm_md.h":15 (ver. 1.10 98/04/22)
// All local includes have been commented out.
*/


#ifndef JVM_MD_H
#define JVM_MD_H

/*
 * This file is currently collecting system-specific dregs for the
 * JNI conversion, which should be sorted out later.
 */

#include <dirent.h>		/* For DIR */
#include <sys/param.h>		/* For MAXPATHLEN */
#include <unistd.h>		/* For F_OK, R_OK, W_OK */
#include <sys/int_types.h>	/* for intptr_t types (64 Bit cleanliness) */

#define JNI_ONLOAD_SYMBOLS      {"JNI_OnLoad"}
#define JNI_ONUNLOAD_SYMBOLS    {"JNI_OnUnload"}
#define JVM_ONLOAD_SYMBOLS      {"JVM_OnLoad"} 
#define AGENT_ONLOAD_SYMBOLS    {"Agent_OnLoad"} 
#define AGENT_ONUNLOAD_SYMBOLS  {"Agent_OnUnload"}

#define JNI_LIB_PREFIX "lib"
#define JNI_LIB_SUFFIX ".so"

#define JVM_MAXPATHLEN MAXPATHLEN

#define JVM_R_OK    R_OK
#define JVM_W_OK    W_OK
#define JVM_X_OK    X_OK
#define JVM_F_OK    F_OK

/*
 * File I/O
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* O Flags */

#define JVM_O_RDONLY     O_RDONLY
#define JVM_O_WRONLY     O_WRONLY
#define JVM_O_RDWR       O_RDWR
#define JVM_O_O_APPEND   O_APPEND
#define JVM_O_EXCL       O_EXCL
#define JVM_O_CREAT      O_CREAT


/* Signal definitions */

#define BREAK_SIGNAL     SIGQUIT           /* Thread dumping support.    */
#define INTERRUPT_SIGNAL SIGUSR1           /* Interruptible I/O support. */
#define ASYNC_SIGNAL     SIGUSR2           /* Watcher & async err support. */
#define SHUTDOWN1_SIGNAL SIGHUP            /* Shutdown Hooks support.    */
#define SHUTDOWN2_SIGNAL SIGINT
#define SHUTDOWN3_SIGNAL SIGTERM
/* alternative signals used with -XX:+UseAltSigs (or for backward 
   compatibility with 1.2, -Xusealtsigs) flag. Chosen to be
   unlikely to conflict with applications embedding the vm */
#define ALT_INTERRUPT_SIGNAL (SIGRTMIN + SIGRTMAX)/2 /* alternate intio signal */
#define ALT_ASYNC_SIGNAL     ALT_INTERRUPT_SIGNAL+1  /* alternate async signal */

/* With 1.4.1 libjsig added versioning: used in os_solaris.cpp and jsig.c */
#define JSIG_VERSION_1_4_1   0x30140100

#endif /* JVM_MD_H */
















