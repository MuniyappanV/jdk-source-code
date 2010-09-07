/*
 * Copyright (c) 2003, 2009, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */


class VM_ReportJavaOutOfMemory;

class VMError : public StackObj {
  friend class VM_ReportJavaOutOfMemory;

  enum ErrorType {
    internal_error = 0xe0000000,
    oom_error      = 0xe0000001
  };
  int          _id;          // Solaris/Linux signals: 0 - SIGRTMAX
                             // Windows exceptions: 0xCxxxxxxx system errors
                             //                     0x8xxxxxxx system warnings

  const char * _message;

  Thread *     _thread;      // NULL if it's native thread


  // additional info for crashes
  address      _pc;          // faulting PC
  void *       _siginfo;     // ExceptionRecord on Windows,
                             // siginfo_t on Solaris/Linux
  void *       _context;     // ContextRecord on Windows,
                             // ucontext_t on Solaris/Linux

  // additional info for VM internal errors
  const char * _filename;
  int          _lineno;

  // used by fatal error handler
  int          _current_step;
  const char * _current_step_info;
  int          _verbose;

  // used by reporting about OOM
  size_t       _size;

  // set signal handlers on Solaris/Linux or the default exception filter
  // on Windows, to handle recursive crashes.
  void reset_signal_handlers();

  // handle -XX:+ShowMessageBoxOnError. buf is used to format the message string
  void show_message_box(char* buf, int buflen);

  // generate an error report
  void report(outputStream* st);

  // accessor
  const char* message()         { return _message; }

public:
  // Constructor for crashes
  VMError(Thread* thread, int sig, address pc, void* siginfo, void* context);
  // Constructor for VM internal errors
  VMError(Thread* thread, const char* message, const char* filename, int lineno);

  // Constructors for VM OOM errors
  VMError(Thread* thread, size_t size, const char* message, const char* filename, int lineno);
  // Constructor for non-fatal errors
  VMError(const char* message);

  // return a string to describe the error
  char *error_string(char* buf, int buflen);

  // main error reporting function
  void report_and_die();

  // reporting OutOfMemoryError
  void report_java_out_of_memory();

  // returns original flags for signal, if it was resetted, or -1 if
  // signal was not changed by error reporter
  static int get_resetted_sigflags(int sig);

  // returns original handler for signal, if it was resetted, or NULL if
  // signal was not changed by error reporter
  static address get_resetted_sighandler(int sig);
};
