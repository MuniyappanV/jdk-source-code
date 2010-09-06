#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)jniTypes_amd64.hpp	1.2 03/12/23 16:35:51 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// This file holds platform-dependent routines used to write primitive
// jni types to the array of arguments passed into JavaCalls::call

class JNITypes : AllStatic 
{
  // These functions write a java primitive type (in native format) to
  // a java stack slot array to be passed as an argument to
  // JavaCalls:calls.  I.e., they are functionally 'push' operations
  // if they have a 'pos' formal parameter.  Note that jlong's and
  // jdouble's are written _in reverse_ of the order in which they
  // appear in the interpreter stack.  This is because call stubs (see
  // stubGenerator_sparc.cpp) reverse the argument list constructed by
  // JavaCallArguments (see javaCalls.hpp).

public:
  // Ints are stored in native format in one JavaCallArgument slot at *to.
  static inline void put_int(jint  from, intptr_t *to)
  {
    *(jint*) to = from; 
  }

  static inline void put_int(jint  from, intptr_t *to, int& pos) 
  {
    *(jint*) (to + pos++) = from;
  }

  static inline void put_int(jint *from, intptr_t *to, int& pos)
  {
    *(jint*) (to + pos++) = *from; 
  }

  // Longs are stored in native format in one JavaCallArgument slot at
  // *(to+1).
  static inline void put_long(jlong  from, intptr_t *to)
  {
    *(jlong*) (to + 1) = from; 
  }

  static inline void put_long(jlong  from, intptr_t *to, int& pos)
  {
    *(jlong*) (to + 1 + pos) = from;
    pos += 2; 
  }

  static inline void put_long(jlong *from, intptr_t *to, int& pos)
  {
    *(jlong*) (to + 1 + pos) = *from;
    pos += 2; 
  }

  // Oops are stored in native format in one JavaCallArgument slot at *to.
  static inline void put_obj(oop  from, intptr_t *to)
  { 
    *(oop*) to = from;
  }

  static inline void put_obj(oop  from, intptr_t *to, int& pos)
  {
    *(oop*) (to + pos++) = from; 
  }

  static inline void put_obj(oop *from, intptr_t *to, int& pos)
  { 
    *(oop*) (to + pos++) = *from;
  }

  // Floats are stored in native format in one JavaCallArgument slot at *to.
  static inline void put_float(jfloat  from, intptr_t *to)
  {
    *(jfloat*) to = from;
  }

  static inline void put_float(jfloat  from, intptr_t *to, int& pos)
  {
    *(jfloat*) (to + pos++) = from;
  }

  static inline void put_float(jfloat *from, intptr_t *to, int& pos)
  {
    *(jfloat*) (to + pos++) = *from;
  }

  // Doubles are stored in native word format in one JavaCallArgument
  // slot at *(to+1).
  static inline void put_double(jdouble  from, intptr_t *to)
  { 
    *(jdouble*) (to + 1) = from; 
  }

  static inline void put_double(jdouble  from, intptr_t *to, int& pos)
  {
    *(jdouble*) (to + 1 + pos) = from;
    pos += 2; 
  }

  static inline void put_double(jdouble *from, intptr_t *to, int& pos)
  {
    *(jdouble*) (to + 1 + pos) = *from;
    pos += 2; 
  }

  // The get_xxx routines, on the other hand, actually _do_ fetch
  // java primitive types from the interpreter stack.
  // No need to worry about alignment on Intel.
  static inline jint get_int(intptr_t *from) 
  {
    return *(jint*) from; 
  }

  static inline jlong get_long(intptr_t *from)
  {
    return *(jlong*) (from + 1); 
  }

  static inline oop get_obj(intptr_t *from) 
  {
    return *(oop*) from;
  }

  static inline jfloat get_float(intptr_t *from)
  {
    return *(jfloat*) from; 
  }

  static inline jdouble get_double(intptr_t *from)
  {
    return *(jdouble*) (from + 1); 
  }
};
