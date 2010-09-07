/*
 * @(#)defines.h	1.14 03/08/01
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

// random definitions

#ifdef _MSC_VER 
#include <windows.h>
#include <winuser.h>
#else
#include <unistd.h>
#endif

#ifndef FULL
#define FULL 1 /* Adds <500 bytes to the zipped final product. */
#endif

#if FULL  // define this if you want debugging and/or compile-time attributes
#define IF_FULL(x) x
#else
#define IF_FULL(x) /*x*/
#endif

#ifdef PRODUCT
#define IF_PRODUCT(xxx) xxx
#define NOT_PRODUCT(xxx)
#define assert(p) (0)
#define printcr false &&
#else
#define IF_PRODUCT(xxx)
#define NOT_PRODUCT(xxx) xxx
#define assert(p) ((p) || (assert_failed(#p), 1))
#define printcr u->verbose && u->printcr_if_verbose
extern "C" void breakpoint();
extern void assert_failed(const char*);
#define BREAK (breakpoint())
#endif

// Build-time control of some C++ inlining.
// To make a slightly faster smaller binary, say "CC -Dmaybe_inline=inline"
#ifndef maybe_inline
#define maybe_inline /*inline*/
#endif
// By marking larger member functions inline, we remove external linkage.
#ifndef local_inline
#define local_inline inline
#endif

// Error messages that we have
#define ERROR_ENOMEM	"Native allocation failed"
#define ERROR_FORMAT	"Corrupted pack file"
#define ERROR_RESOURCE	"Cannot extract resource file"
#define ERROR_OVERFLOW  "Internal buffer overflow"
#define ERROR_INTERNAL	"Internal error"

#define LOGFILE_STDOUT "-"
#define LOGFILE_STDERR ""

#define lengthof(array) (sizeof(array)/sizeof(array[0]))

#define NEW(T, n)    (T*) must_malloc(sizeof(T)*(n))
#define U_NEW(T, n)  (T*) u->alloc(sizeof(T)*(n))
#define T_NEW(T, n)  (T*) u->temp_alloc(sizeof(T)*(n))


// bytes and byte arrays

typedef unsigned int uint;
#ifdef _LP64
typedef unsigned int uLong; // Historical zlib, should be 32-bit.
#else
typedef unsigned long uLong;
#endif
#ifdef _MSC_VER 
typedef LONGLONG 	jlong;
typedef DWORDLONG 	julong;
#define MKDIR(dir) 	mkdir(dir)
#define getpid() 	_getpid()
#define PATH_MAX 	MAX_PATH
#define dup2(a,b)	_dup2(a,b)
#define strcasecmp(s1, s2) _stricmp(s1,s2)
#define tempname	_tempname
#define sleep Sleep
#else
typedef signed char byte;
#ifdef _LP64
typedef long jlong;
typedef long unsigned julong;
#else
typedef long long jlong;
typedef long long unsigned julong;
#endif
#define MKDIR(dir) mkdir(dir, 0777);
#endif

#ifdef OLDCC
typedef int bool;
enum { false, true };
#endif

#define null (0)

#ifndef __sparc 
#define intptr_t jlong
#endif

#define ptrlowbits(x)  ((int) (intptr_t)(x))


// Keys used by Java:
#define UNPACK_DEFLATE_HINT		"unpack.deflate.hint"

#define COM_PREFIX			"com.sun.java.util.jar.pack."
#define UNPACK_MODIFICATION_TIME	COM_PREFIX"unpack.modification.time"
#define DEBUG_VERBOSE			COM_PREFIX"verbose"

#define ZIP_ARCHIVE_MARKER_COMMENT	"PACK200"

// The following are not known to the Java classes:
#define UNPACK_LOG_FILE			COM_PREFIX"unpack.log.file"
#define UNPACK_REMOVE_PACKFILE		COM_PREFIX"unpack.remove.packfile"


// Called from unpacker layers
#define _CHECK_DO(t,x)	 	{ if (t) {x;} }

#define CHECK			_CHECK_DO(aborting(), return)
#define CHECK_(y)		_CHECK_DO(aborting(), return y)
#define CHECK_0			_CHECK_DO(aborting(), return 0)

#define CHECK_NULL(p)		_CHECK_DO((p)==null, return)
#define CHECK_NULL_(y,p)	_CHECK_DO((p)==null, return y)
#define CHECK_NULL_0(p)		_CHECK_DO((p)==null, return 0)

#define STR_TRUE   "true"
#define STR_FALSE  "false"

#define STR_TF(x)  ((x) ?  STR_TRUE : STR_FALSE)
#define BOOL_TF(x) (((x) != null && strcmp((x),STR_TRUE) == 0) ? true : false)

#define DEFAULT_ARCHIVE_MODTIME 1060000000 // Aug 04, 2003 5:26 PM PDT
