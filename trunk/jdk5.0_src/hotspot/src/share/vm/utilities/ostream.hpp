#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)ostream.hpp	1.33 04/03/02 17:17:01 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Output streams for printing
// 
// Printing guidelines:
// Where possible, please use tty->print() and tty->print_cr().
// For product mode VM warnings use warning() which internally uses tty.
// In places where tty is not initialized yet or too much overhead,
// we may use jio_printf:
//     jio_fprintf(defaultStream::output_stream(), "Message");
// This allows for redirection via -XX:+DisplayVMOutputToStdout and
// -XX:+DisplayVMOutputToStderr
class outputStream : public ResourceObj {
 protected:
   int _indentation; // current indentation
   int _width;       // width of the page
   int _position;    // position on the current line
   int _newlines;    // number of '\n' output so far
   julong _precount; // number of chars output, less _position
   TimeStamp _stamp; // for time stamps

   void update_position(const char* s, size_t len);
   static const char* do_vsnprintf(char* buffer, size_t buflen,
                                   const char* format, va_list ap,
                                   bool add_cr,
                                   size_t& result_len);

 public:
   // creation
   outputStream(int width = 80);
   outputStream(int width, bool has_time_stamps);

   // indentation
   void indent();
   void inc() { _indentation++; };
   void dec() { _indentation--; };
   int  indentation() const    { return _indentation; }
   void set_indentation(int i) { _indentation = i;    }
   void fill_to(int col);

   // sizing
   int width()    const { return _width;    }
   int position() const { return _position; }
   int newlines() const { return _newlines; }
   julong count() const { return _precount + _position; }
   void set_count(julong count) { _precount = count - _position; }
   void set_position(int pos)   { _position = pos; }

   // printing
   void print(const char* format, ...);
   void print_cr(const char* format, ...);
   void vprint(const char *format, va_list argptr);
   void vprint_cr(const char* format, va_list argptr);
   void print_raw(const char* str)            { write(str, strlen(str)); }
   void print_raw(const char* str, int len)   { write(str,         len); }
   void print_raw_cr(const char* str)         { write(str, strlen(str)); cr(); }
   void print_raw_cr(const char* str, int len){ write(str,         len); cr(); }
   void put(char ch);
   void sp();
   void cr();
   void bol() { if (_position > 0)  cr(); }

   // Time stamp
   TimeStamp& time_stamp() { return _stamp; }
   void stamp();

   // portable printing of 64 bit integers
   void print_jlong(jlong value);
   void print_julong(julong value);

   // flushing
   virtual void flush() {}
   virtual void write(const char* str, size_t len) = 0;
   virtual ~outputStream() {}  // close properly on deletion

   void dec_cr() { dec(); cr(); }
   void inc_cr() { inc(); cr(); }
};

// standard output
				// ANSI C++ name collision
extern outputStream* tty;	    // tty output
extern outputStream* gclog_or_tty;  // stream for gc log if -Xloggc:<f>, or tty

// advisory locking for the shared tty stream:
class ttyLocker: StackObj {
 private:
  intx _holder;

 public:
  static intx  hold_tty();                // returns a "holder" token
  static void  release_tty(intx holder);  // must witness same token
  static void  break_tty_lock_for_safepoint(intx holder);

  ttyLocker()  { _holder = hold_tty(); }
  ~ttyLocker() { release_tty(_holder); }
};

// for writing to strings; buffer will expand automatically
class stringStream : public outputStream {
 protected:
  char*  buffer;
  size_t buffer_pos;
  size_t buffer_length;
  bool   buffer_fixed;
 public:
  stringStream(size_t initial_bufsize = 256);
  stringStream(char* fixed_buffer, size_t fixed_buffer_size);
  ~stringStream();
  virtual void write(const char* c, size_t len);
  size_t fixed_buffer_size() const { return buffer_fixed ? buffer_length : -1; }
  size_t      size() { return buffer_pos; }
  const char* base() { return buffer; }
  void  reset() { buffer_pos = 0; _precount = 0; _position = 0; }
  char* as_string();
};

class fileStream : public outputStream {
 protected:
  FILE* _file;
  bool  _need_close;
 public:
  fileStream(const char* file_name);
  fileStream(FILE* file) { _file = file; _need_close = false; }
  ~fileStream();
  bool is_open() const { return _file != NULL; }
  virtual void write(const char* c, size_t len);
  void flush();
};

// unlike fileStream, fdStream does unbuffered I/O by calling
// open() and write() directly. It is async-safe, but output
// from multiple thread may be mixed together. Used by fatal
// error handler.
class fdStream : public outputStream {
 protected:
  int  _fd;
  bool _need_close;
 public:
  fdStream(const char* file_name);
  fdStream(int fd = -1) { _fd = fd; _need_close = false; }
  ~fdStream();
  bool is_open() const { return _fd != -1; }
  void set_fd(int fd) { _fd = fd; _need_close = false; }
  int fd() const { return _fd; }
  virtual void write(const char* c, size_t len);
  void flush() {};
};

void ostream_init();
void ostream_init_log();
void ostream_exit();
void ostream_abort();

// staticBufferStream uses a user-supplied buffer for all formatting.
// Used for safe formatting during fatal error handling.  Not MT safe.
// Do not share the stream between multiple threads.
class staticBufferStream : public outputStream {
 private:
  char* _buffer;
  size_t _buflen;
  outputStream* _outer_stream;
 public:
  staticBufferStream(char* buffer, size_t buflen,
		     outputStream *outer_stream);
  ~staticBufferStream() {};
  virtual void write(const char* c, size_t len);
  void flush();
  void print(const char* format, ...);
  void print_cr(const char* format, ...);
  void vprint(const char *format, va_list argptr);
  void vprint_cr(const char* format, va_list argptr);
};

#define O_BUFLEN 2000   // max size of output of individual print() methods
