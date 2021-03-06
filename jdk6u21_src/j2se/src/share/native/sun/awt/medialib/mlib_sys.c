/*
 * @(#)mlib_sys.c	1.18 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
  

#ifdef __SUNPRO_C
#pragma ident	"@(#)mlib_sys.c	1.13	03/01/14 SMI"
#endif /* __SUNPRO_C */

#include <stdlib.h>
#include <string.h>
#include <mlib_types.h>
#include <mlib_sys_proto.h>
#include "mlib_SysMath.h"

/***************************************************************/

#if ! defined ( __MEDIALIB_OLD_NAMES )
#if defined ( __SUNPRO_C )

#pragma weak mlib_memmove = __mlib_memmove
#pragma weak mlib_malloc = __mlib_malloc
#pragma weak mlib_realloc = __mlib_realloc
#pragma weak mlib_free = __mlib_free
#pragma weak mlib_memset = __mlib_memset
#pragma weak mlib_memcpy = __mlib_memcpy

#ifdef MLIB_NO_LIBSUNMATH
#pragma weak mlib_sincosf = __mlib_sincosf
#endif /* MLIB_NO_LIBSUNMATH */

#elif defined ( __GNUC__ ) /* defined ( __SUNPRO_C ) */

  __typeof__ ( __mlib_memmove) mlib_memmove
    __attribute__ ((weak,alias("__mlib_memmove")));
  __typeof__ ( __mlib_malloc) mlib_malloc
    __attribute__ ((weak,alias("__mlib_malloc")));
  __typeof__ ( __mlib_realloc) mlib_realloc
    __attribute__ ((weak,alias("__mlib_realloc")));
  __typeof__ ( __mlib_free) mlib_free
    __attribute__ ((weak,alias("__mlib_free")));
  __typeof__ ( __mlib_memset) mlib_memset
    __attribute__ ((weak,alias("__mlib_memset")));
  __typeof__ ( __mlib_memcpy) mlib_memcpy
    __attribute__ ((weak,alias("__mlib_memcpy")));

#ifdef MLIB_NO_LIBSUNMATH
 
void __mlib_sincosf (float x, float *s, float *c);

__typeof__ ( __mlib_sincosf) mlib_sincosf
    __attribute__ ((weak,alias("__mlib_sincosf")));
#endif /* MLIB_NO_LIBSUNMATH */

#else /* defined ( __SUNPRO_C ) */

#error  "unknown platform"

#endif /* defined ( __SUNPRO_C ) */
#endif /* ! defined ( __MEDIALIB_OLD_NAMES ) */

/***************************************************************/

void *__mlib_malloc(mlib_u32 size)
{
#ifdef _MSC_VER
  /*
   * Currently, all MS C compilers for Win32 platforms default to 8 byte
   * alignment. -- from stdlib.h of MS VC++5.0.
   */
  return (void *) malloc(size);
#else /* _MSC_VER */
  return (void *) memalign(8, size);
#endif /* _MSC_VER */
}

void *__mlib_realloc(void *ptr, mlib_u32 size)
{
  return realloc(ptr, size);
}

void __mlib_free(void *ptr)
{
  free(ptr);
}

void *__mlib_memset(void *s, mlib_s32 c, mlib_u32 n)
{
  return memset(s, c, n);
}

void *__mlib_memcpy(void *s1, void *s2, mlib_u32 n)
{
  return memcpy(s1, s2, n);
}

void *__mlib_memmove(void *s1, void *s2, mlib_u32 n)
{
  return memmove(s1, s2, n);
}

#ifdef MLIB_NO_LIBSUNMATH

void __mlib_sincosf (mlib_f32 x, mlib_f32 *s, mlib_f32 *c)
{
  *s = (mlib_f32)sin(x);
  *c = (mlib_f32)cos(x);
}

#endif /* MLIB_NO_LIBSUNMATH */
