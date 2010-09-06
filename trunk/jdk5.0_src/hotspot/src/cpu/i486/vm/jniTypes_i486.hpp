#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)jniTypes_i486.hpp	1.14 03/12/23 16:36:21 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// This file holds platform-dependent routines used to write primitive jni 
// types to the array of arguments passed into JavaCalls::call

class JNITypes : AllStatic {
  // These functions write a java primitive type (in native format)
  // to a java stack slot array to be passed as an argument to JavaCalls:calls.
  // I.e., they are functionally 'push' operations if they have a 'pos'
  // formal parameter.  Note that jlong's and jdouble's are written
  // _in reverse_ of the order in which they appear in the interpreter
  // stack.  This is because call stubs (see stubGenerator_sparc.cpp)
  // reverse the argument list constructed by JavaCallArguments (see
  // javaCalls.hpp).

private:
  // Helper routines.
  static inline void    put_int2r(jint *from, intptr_t *to)	      { *(jint *)(to++) = from[1];
                                                                        *(jint *)(to  ) = from[0]; }
  static inline void    put_int2r(jint *from, intptr_t *to, int& pos) { put_int2r(from, to + pos); pos += 2; }

public:
  // Ints are stored in native format in one JavaCallArgument slot at *to.
  static inline void    put_int(jint  from, intptr_t *to)	    { *(jint *)(to +   0  ) =  from; }
  static inline void    put_int(jint  from, intptr_t *to, int& pos) { *(jint *)(to + pos++) =  from; }
  static inline void    put_int(jint *from, intptr_t *to, int& pos) { *(jint *)(to + pos++) = *from; }

  // Longs are stored in big-endian word format in two JavaCallArgument slots at *to.
  // The high half is in *to and the low half in *(to+1).
  static inline void    put_long(jlong  from, intptr_t *to)	      { put_int2r((jint *)&from, to); }
  static inline void    put_long(jlong  from, intptr_t *to, int& pos) { put_int2r((jint *)&from, to, pos); }
  static inline void    put_long(jlong *from, intptr_t *to, int& pos) { put_int2r((jint *) from, to, pos); }

  // Oops are stored in native format in one JavaCallArgument slot at *to.
  static inline void    put_obj(oop  from, intptr_t *to)	   { *(oop *)(to +   0  ) =  from; }
  static inline void    put_obj(oop  from, intptr_t *to, int& pos) { *(oop *)(to + pos++) =  from; }
  static inline void    put_obj(oop *from, intptr_t *to, int& pos) { *(oop *)(to + pos++) = *from; }

  // Floats are stored in native format in one JavaCallArgument slot at *to.
  static inline void    put_float(jfloat  from, intptr_t *to)   	{ *(jfloat *)(to +   0  ) =  from;  }
  static inline void    put_float(jfloat  from, intptr_t *to, int& pos) { *(jfloat *)(to + pos++) =  from; }
  static inline void    put_float(jfloat *from, intptr_t *to, int& pos) { *(jfloat *)(to + pos++) = *from; }

  // Doubles are stored in big-endian word format in two JavaCallArgument slots at *to.
  // The high half is in *to and the low half in *(to+1).
  static inline void    put_double(jdouble  from, intptr_t *to)	          { put_int2r((jint *)&from, to); }
  static inline void    put_double(jdouble  from, intptr_t *to, int& pos) { put_int2r((jint *)&from, to, pos); }
  static inline void    put_double(jdouble *from, intptr_t *to, int& pos) { put_int2r((jint *) from, to, pos); }


  // The get_xxx routines, on the other hand, actually _do_ fetch
  // java primitive types from the interpreter stack.
  // No need to worry about alignment on Intel.
  static inline jint    get_int   (intptr_t *from) { return *(jint *)   from; }
  static inline jlong   get_long  (intptr_t *from) { return *(jlong *)  from; }
  static inline oop     get_obj   (intptr_t *from) { return *(oop *)    from; }
  static inline jfloat  get_float (intptr_t *from) { return *(jfloat *) from; }
  static inline jdouble get_double(intptr_t *from) { return *(jdouble *)from; }
};
