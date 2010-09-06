#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)jvm_linux.cpp	1.12 03/12/23 16:37:30 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_jvm_linux.cpp.incl"

#include <signal.h>

/*
 * FIXME: This is temporary hack to keep Linux Runtime.exec()
 * code happy. See $JDK/src/linux/native/java/lang/UnixProcess_md.c
 */
int _JVM_native_threads = 1;

// sun.misc.Signal ///////////////////////////////////////////////////////////
// Signal code is mostly copied from classic vm, signals_md.c	1.4 98/08/23
/*
 * This function is included primarily as a debugging aid. If Java is
 * running in a console window, then pressing <CTRL-\\> will cause
 * the current state of all active threads and monitors to be written
 * to the console window.
 */

JVM_ENTRY_NO_ENV(void*, JVM_RegisterSignal(jint sig, void* handler))
  // Copied from classic vm
  // @(#)signals_md.c	1.4 98/08/23
  void* newHandler = handler == (void *)2
                   ? os::user_handler()
                   : handler;
  switch (sig) {
    /* The following are already used by the VM. */
    case INTERRUPT_SIGNAL:
    case SIGFPE:
    case SIGILL:
    case SIGSEGV:

    /* The following signal is used by the VM to dump thread stacks unless
       ReduceSignalUsage is set, in which case the user is allowed to set
       his own _native_ handler for this signal; thus, in either case,
       we do not allow JVM_RegisterSignal to change the handler. */
    case BREAK_SIGNAL:
      return (void *)-1;

    /* The following signals are used for Shutdown Hooks support. However, if
       ReduceSignalUsage (-Xrs) is set, Shutdown Hooks must be invoked via 
       System.exit(), Java is not allowed to use these signals, and the the 
       user is allowed to set his own _native_ handler for these signals and
       invoke System.exit() as needed. Terminator.setup() is avoiding 
       registration of these signals when -Xrs is present. 
       - If the HUP signal is ignored (from the nohup) command, then Java
         is not allowed to use this signal.
     */ 

    case SHUTDOWN1_SIGNAL:
    case SHUTDOWN2_SIGNAL:
    case SHUTDOWN3_SIGNAL:
      if (ReduceSignalUsage) return (void*)-1;
      if (os::Linux::is_sig_ignored(sig)) return (void*)1;
  }

  void* oldHandler = os::signal(sig, newHandler);
  if (oldHandler == os::user_handler()) {
      return (void *)2;
  } else {
      return oldHandler;
  }
JVM_END


JVM_ENTRY_NO_ENV(jboolean, JVM_RaiseSignal(jint sig))
  if (ReduceSignalUsage) {
    // do not allow SHUTDOWN1_SIGNAL,SHUTDOWN2_SIGNAL,SHUTDOWN3_SIGNAL,
    // BREAK_SIGNAL to be raised when ReduceSignalUsage is set, since
    // no handler for them is actually registered in JVM or via
    // JVM_RegisterSignal.
    if (sig == SHUTDOWN1_SIGNAL || sig == SHUTDOWN2_SIGNAL ||
        sig == SHUTDOWN3_SIGNAL || sig == BREAK_SIGNAL) {
      return JNI_FALSE;
    }
  }
  else if ((sig == SHUTDOWN1_SIGNAL || sig == SHUTDOWN2_SIGNAL ||
            sig == SHUTDOWN3_SIGNAL) && os::Linux::is_sig_ignored(sig)) {
    // do not allow SHUTDOWN1_SIGNAL to be raised when SHUTDOWN1_SIGNAL
    // is ignored, since no handler for them is actually registered in JVM
    // or via JVM_RegisterSignal.
    // This also applies for SHUTDOWN2_SIGNAL and SHUTDOWN3_SIGNAL
    return JNI_FALSE;
  }

  os::signal_raise(sig);
  return JNI_TRUE;
JVM_END

/* 
  All the defined signal names for Linux. 

  NOTE that not all of these names are accepted by our Java implementation

  Via an existing claim by the VM, sigaction restrictions, or
  the "rules of Unix" some of these names will be rejected at runtime.
  For example the VM sets up to handle USR1, sigaction returns EINVAL for 
  STOP, and Linux simply doesn't allow catching of KILL.

  Here are the names currently accepted by a user of sun.misc.Signal with 
  1.4.1 (ignoring potential interaction with use of chaining, etc):

    HUP, INT, TRAP, ABRT, IOT, BUS, USR2, PIPE, ALRM, TERM, STKFLT,
    CLD, CHLD, CONT, TSTP, TTIN, TTOU, URG, XCPU, XFSZ, VTALRM, PROF,
    WINCH, POLL, IO, PWR, SYS

*/

struct siglabel {
  char *name;
  int   number;
};

struct siglabel siglabels[] =
  /* derived from /usr/include/bits/signum.h on RH7.2 */
  {"HUP",	SIGHUP,		/* Hangup (POSIX).  */
  "INT",	SIGINT,		/* Interrupt (ANSI).  */
  "QUIT",	SIGQUIT,	/* Quit (POSIX).  */
  "ILL",	SIGILL,		/* Illegal instruction (ANSI).  */
  "TRAP",	SIGTRAP,	/* Trace trap (POSIX).  */
  "ABRT",	SIGABRT,	/* Abort (ANSI).  */
  "IOT",	SIGIOT,		/* IOT trap (4.2 BSD).  */
  "BUS",	SIGBUS,		/* BUS error (4.2 BSD).  */
  "FPE",	SIGFPE,		/* Floating-point exception (ANSI).  */
  "KILL",	SIGKILL,	/* Kill, unblockable (POSIX).  */
  "USR1",	SIGUSR1,	/* User-defined signal 1 (POSIX).  */
  "SEGV",	SIGSEGV,	/* Segmentation violation (ANSI).  */
  "USR2",	SIGUSR2,	/* User-defined signal 2 (POSIX).  */
  "PIPE",	SIGPIPE,	/* Broken pipe (POSIX).  */
  "ALRM",	SIGALRM,	/* Alarm clock (POSIX).  */
  "TERM",	SIGTERM,	/* Termination (ANSI).  */
  "STKFLT",	SIGSTKFLT,	/* Stack fault.  */
  "CLD",	SIGCLD,		/* Same as SIGCHLD (System V).  */
  "CHLD",	SIGCHLD,	/* Child status has changed (POSIX).  */
  "CONT",	SIGCONT,	/* Continue (POSIX).  */
  "STOP",	SIGSTOP,	/* Stop, unblockable (POSIX).  */
  "TSTP",	SIGTSTP,	/* Keyboard stop (POSIX).  */
  "TTIN",	SIGTTIN,	/* Background read from tty (POSIX).  */
  "TTOU",	SIGTTOU,	/* Background write to tty (POSIX).  */
  "URG",	SIGURG,		/* Urgent condition on socket (4.2 BSD).  */
  "XCPU",	SIGXCPU,	/* CPU limit exceeded (4.2 BSD).  */
  "XFSZ",	SIGXFSZ,	/* File size limit exceeded (4.2 BSD).  */
  "VTALRM",	SIGVTALRM,	/* Virtual alarm clock (4.2 BSD).  */
  "PROF",	SIGPROF,	/* Profiling alarm clock (4.2 BSD).  */
  "WINCH",	SIGWINCH,	/* Window size change (4.3 BSD, Sun).  */
  "POLL",	SIGPOLL,	/* Pollable event occurred (System V).  */
  "IO",		SIGIO,		/* I/O now possible (4.2 BSD).  */
  "PWR",	SIGPWR,		/* Power failure restart (System V).  */
#ifdef SIGSYS
  "SYS",	SIGSYS		/* Bad system call. Only on some Linuxen! */
#endif
  };

JVM_ENTRY_NO_ENV(jint, JVM_FindSignal(const char *name))

  /* find and return the named signal's number */

  for(int i=0;i<sizeof(siglabels)/sizeof(struct siglabel);i++)
    if(!strcmp(name, siglabels[i].name))  
      return siglabels[i].number;

  return -1;

JVM_END


// Reconciliation History
// jvm_solaris.cpp	1.7 99/08/30 18:13:04
// End
