#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)unsafe.cpp	1.42 04/08/02 13:07:19 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

/*
 *      Implementation of class sun.misc.Unsafe
 */

#include "incls/_precompiled.incl"
#include "incls/_unsafe.cpp.incl"

#define MAX_OBJECT_SIZE \
  ( arrayOopDesc::header_size(T_DOUBLE) * HeapWordSize \
    + ((julong)max_jint * sizeof(double)) )


#define UNSAFE_ENTRY(result_type, header) \
  JVM_ENTRY(result_type, header)

// Can't use UNSAFE_LEAF because it has the signature of a straight
// call into the runtime (just like JVM_LEAF, funny that) but it's
// called like a Java Native and thus the wrapper built for it passes
// arguments like a JNI call.  It expects those arguments to be popped
// from the stack on Intel like all good JNI args are, and adjusts the
// stack according.  Since the JVM_LEAF call expects no extra
// arguments the stack isn't popped in the C code, is pushed by the
// wrapper and we get sick.
//#define UNSAFE_LEAF(result_type, header) \
//  JVM_LEAF(result_type, header)

#define UNSAFE_END JVM_END

#define UnsafeWrapper(arg) /*nothing, for the present*/


inline void* addr_from_java(jlong addr) {
  // This assert fails in a variety of ways on 32-bit systems.
  // It is impossible to predict whether native code that converts
  // pointers to longs will sign-extend or zero-extend the addresses.
  //assert(addr == (uintptr_t)addr, "must not be odd high bits");
  return (void*)(uintptr_t)addr;
}

inline jlong addr_to_java(void* p) {
  assert(p == (void*)(uintptr_t)p, "must not be odd high bits");
  return (uintptr_t)p;
}


// Note: The VM's obj_field and related accessors use byte-scaled
// ("unscaled") offsets, just as the unsafe methods do.

// However, the method Unsafe.fieldOffset explicitly declines to
// guarantee this.  The field offset values manipulated by the Java user
// through the Unsafe API are opaque cookies that just happen to be byte
// offsets.  We represent this state of affairs by passing the cookies
// through conversion functions when going between the VM and the Unsafe API.
// The conversion functions just happen to be no-ops at present.

inline jlong field_offset_to_byte_offset(jlong field_offset) {
  return field_offset;
}

inline jlong field_offset_from_byte_offset(jlong byte_offset) {
  return byte_offset;
}

inline jint invocation_key_from_method_slot(jint slot) {
  return slot;
}

inline jint invocation_key_to_method_slot(jint key) {
  return key;
}

inline void* index_oop_from_field_offset_long(oop p, jlong field_offset) {
  jlong byte_offset = field_offset_to_byte_offset(field_offset);
#ifdef ASSERT
  if (p != NULL) {
    assert(byte_offset >= 0 && byte_offset <= (jlong)MAX_OBJECT_SIZE, "sane offset");
    if (byte_offset == (jint)byte_offset) {
      void* ptr_plus_disp = (char*)p + byte_offset;
      assert((void*)p->obj_field_addr((jint)byte_offset) == ptr_plus_disp,
	     "raw [ptr+disp] must be consistent with oop::field_base");
    }
  }
#endif
  if (sizeof(char*) == sizeof(jint))    // (this constant folds!)
    return (char*)p + (jint) byte_offset;
  else
    return (char*)p +        byte_offset;
}

// Externally callable versions:
// (Use these in compiler intrinsics which emulate unsafe primitives.)
jlong Unsafe_field_offset_to_byte_offset(jlong field_offset) {
  return field_offset;
}
jlong Unsafe_field_offset_from_byte_offset(jlong byte_offset) {
  return byte_offset;
}
jint Unsafe_invocation_key_from_method_slot(jint slot) {
  return invocation_key_from_method_slot(slot);
}
jint Unsafe_invocation_key_to_method_slot(jint key) {
  return invocation_key_to_method_slot(key);
}


///// Data in the Java heap.

#define GET_FIELD(obj, offset, type_name, v) \
  oop p = JNIHandles::resolve(obj); \
  type_name v = *(type_name*)index_oop_from_field_offset_long(p, offset)

#define SET_FIELD(obj, offset, type_name, x) \
  oop p = JNIHandles::resolve(obj); \
  *(type_name*)index_oop_from_field_offset_long(p, offset) = x

#define GET_FIELD_VOLATILE(obj, offset, type_name, v) \
  oop p = JNIHandles::resolve(obj); \
  type_name v = *(volatile type_name*)index_oop_from_field_offset_long(p, offset)

#define SET_FIELD_VOLATILE(obj, offset, type_name, x) \
  oop p = JNIHandles::resolve(obj); \
  *(volatile type_name*)index_oop_from_field_offset_long(p, offset) = x; \
  OrderAccess::fence();

// Get/SetObject must be special-cased, since it works with handles.

// The xxx140 variants for backward compatibility do not allow a full-width offset.
UNSAFE_ENTRY(jobject, Unsafe_GetObject140(JNIEnv *env, jobject unsafe, jobject obj, jint offset))
  UnsafeWrapper("Unsafe_GetObject");
  if (obj == NULL)  THROW_0(vmSymbols::java_lang_NullPointerException());
  GET_FIELD(obj, offset, oop, v);
  return JNIHandles::make_local(env, v);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetObject140(JNIEnv *env, jobject unsafe, jobject obj, jint offset, jobject x_h))
  UnsafeWrapper("Unsafe_SetObject");
  if (obj == NULL)  THROW(vmSymbols::java_lang_NullPointerException());
  oop x = JNIHandles::resolve(x_h);
  //SET_FIELD(obj, offset, oop, x);
  oop p = JNIHandles::resolve(obj);
  if (x != NULL) {
    // If there is a heap base pointer, we are obliged to emit a store barrier.
    oop_store((oop*)index_oop_from_field_offset_long(p, offset), x);
  } else {
    *(oop*)index_oop_from_field_offset_long(p, offset) = x;
  }
UNSAFE_END

// The normal variants allow a null base pointer with an arbitrary address.
// But if the base pointer is non-null, the offset should make some sense.
// That is, it should be in the range [0, MAX_OBJECT_SIZE].
UNSAFE_ENTRY(jobject, Unsafe_GetObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset))
  UnsafeWrapper("Unsafe_GetObject");
  GET_FIELD(obj, offset, oop, v);
  return JNIHandles::make_local(env, v);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h))
  UnsafeWrapper("Unsafe_SetObject");
  oop x = JNIHandles::resolve(x_h);
  oop p = JNIHandles::resolve(obj);
  oop_store((oop*)index_oop_from_field_offset_long(p, offset), x);
UNSAFE_END

UNSAFE_ENTRY(jobject, Unsafe_GetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset))
  UnsafeWrapper("Unsafe_GetObjectVolatile");
  GET_FIELD_VOLATILE(obj, offset, oop, v);
  return JNIHandles::make_local(env, v);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h))
  UnsafeWrapper("Unsafe_SetObjectVolatile");
  oop x = JNIHandles::resolve(x_h);
  oop p = JNIHandles::resolve(obj);
  oop_store((oop*)index_oop_from_field_offset_long(p, offset), x);
  OrderAccess::fence();
UNSAFE_END

// Volatile long versions must use locks if !VM_Version::supports_cx8().
// support_cx8 is a surrogate for 'supports atomic long memory ops'.

UNSAFE_ENTRY(jlong, Unsafe_GetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset))
  UnsafeWrapper("Unsafe_GetLongVolatile");
  {
    if (VM_Version::supports_cx8()) {
      GET_FIELD_VOLATILE(obj, offset, jlong, v);
      return v; 
    }
    else {
      oop p = JNIHandles::resolve(obj);
      jlong* addr = (jlong*)(index_oop_from_field_offset_long(p, offset));
      ObjectLocker ol(p, THREAD);
      jlong value = *addr;
      return value;
    }
  }
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong x))
  UnsafeWrapper("Unsafe_SetLongVolatile");
  {
    if (VM_Version::supports_cx8()) {
      SET_FIELD_VOLATILE(obj, offset, jlong, x);
    }
    else {
      oop p = JNIHandles::resolve(obj);
      jlong* addr = (jlong*)(index_oop_from_field_offset_long(p, offset));
      ObjectLocker ol(p, THREAD);
      *addr = x;
    }
  }
UNSAFE_END


#define DEFINE_GETSETOOP(jboolean, Boolean) \
 \
UNSAFE_ENTRY(jboolean, Unsafe_Get##Boolean##140(JNIEnv *env, jobject unsafe, jobject obj, jint offset)) \
  UnsafeWrapper("Unsafe_Get"#Boolean); \
  if (obj == NULL)  THROW_0(vmSymbols::java_lang_NullPointerException()); \
  GET_FIELD(obj, offset, jboolean, v); \
  return v; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_Set##Boolean##140(JNIEnv *env, jobject unsafe, jobject obj, jint offset, jboolean x)) \
  UnsafeWrapper("Unsafe_Set"#Boolean); \
  if (obj == NULL)  THROW(vmSymbols::java_lang_NullPointerException()); \
  SET_FIELD(obj, offset, jboolean, x); \
UNSAFE_END \
 \
UNSAFE_ENTRY(jboolean, Unsafe_Get##Boolean(JNIEnv *env, jobject unsafe, jobject obj, jlong offset)) \
  UnsafeWrapper("Unsafe_Get"#Boolean); \
  GET_FIELD(obj, offset, jboolean, v); \
  return v; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_Set##Boolean(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jboolean x)) \
  UnsafeWrapper("Unsafe_Set"#Boolean); \
  SET_FIELD(obj, offset, jboolean, x); \
UNSAFE_END \
 \
// END DEFINE_GETSETOOP.


#define DEFINE_GETSETOOP_VOLATILE(jboolean, Boolean) \
 \
UNSAFE_ENTRY(jboolean, Unsafe_Get##Boolean##Volatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset)) \
  UnsafeWrapper("Unsafe_Get"#Boolean); \
  GET_FIELD_VOLATILE(obj, offset, jboolean, v); \
  return v; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_Set##Boolean##Volatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jboolean x)) \
  UnsafeWrapper("Unsafe_Set"#Boolean); \
  SET_FIELD_VOLATILE(obj, offset, jboolean, x); \
UNSAFE_END \
 \
// END DEFINE_GETSETOOP_VOLATILE.

DEFINE_GETSETOOP(jboolean, Boolean)
DEFINE_GETSETOOP(jbyte, Byte)
DEFINE_GETSETOOP(jshort, Short);
DEFINE_GETSETOOP(jchar, Char);
DEFINE_GETSETOOP(jint, Int);
DEFINE_GETSETOOP(jlong, Long);
DEFINE_GETSETOOP(jfloat, Float);
DEFINE_GETSETOOP(jdouble, Double);

DEFINE_GETSETOOP_VOLATILE(jboolean, Boolean)
DEFINE_GETSETOOP_VOLATILE(jbyte, Byte)
DEFINE_GETSETOOP_VOLATILE(jshort, Short);
DEFINE_GETSETOOP_VOLATILE(jchar, Char);
DEFINE_GETSETOOP_VOLATILE(jint, Int);
// no long -- handled specially
DEFINE_GETSETOOP_VOLATILE(jfloat, Float);
DEFINE_GETSETOOP_VOLATILE(jdouble, Double);

#undef DEFINE_GETSETOOP


////// Data in the C heap.

// Note:  These do not throw NullPointerException for bad pointers.
// They just crash.  Only a oop base pointer can generate a NullPointerException.

#define DEFINE_GETSETNATIVE(jbyte, Byte, signed_char) \
 \
UNSAFE_ENTRY(jbyte, Unsafe_GetNative##Byte(JNIEnv *env, jobject unsafe, jlong addr)) \
  UnsafeWrapper("Unsafe_GetNative"#Byte); \
  void* p = addr_from_java(addr); \
  JavaThread* t = JavaThread::current(); \
  t->set_doing_unsafe_access(true); \
  jbyte x = *(signed_char*)p; \
  t->set_doing_unsafe_access(false); \
  return x; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_SetNative##Byte(JNIEnv *env, jobject unsafe, jlong addr, jbyte x)) \
  UnsafeWrapper("Unsafe_SetNative"#Byte); \
  JavaThread* t = JavaThread::current(); \
  t->set_doing_unsafe_access(true); \
  void* p = addr_from_java(addr); \
  *(signed_char*)p = x; \
  t->set_doing_unsafe_access(false); \
UNSAFE_END \
 \
// END DEFINE_GETSETNATIVE.

DEFINE_GETSETNATIVE(jbyte, Byte, signed char)
DEFINE_GETSETNATIVE(jshort, Short, signed short);
DEFINE_GETSETNATIVE(jchar, Char, unsigned short);
DEFINE_GETSETNATIVE(jint, Int, jint);
DEFINE_GETSETNATIVE(jlong, Long, jlong);
DEFINE_GETSETNATIVE(jfloat, Float, float);
DEFINE_GETSETNATIVE(jdouble, Double, double);

#undef DEFINE_GETSETNATIVE


UNSAFE_ENTRY(jlong, Unsafe_GetNativeAddress(JNIEnv *env, jobject unsafe, jlong addr))
  UnsafeWrapper("Unsafe_GetNativeAddress");
  void* p = addr_from_java(addr);
  return addr_to_java(*(void**)p);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetNativeAddress(JNIEnv *env, jobject unsafe, jlong addr, jlong x))
  UnsafeWrapper("Unsafe_SetNativeAddress");
  void* p = addr_from_java(addr);
  *(void**)p = addr_from_java(x);
UNSAFE_END


////// Allocation requests

UNSAFE_ENTRY(jobject, Unsafe_AllocateInstance(JNIEnv *env, jobject unsafe, jclass cls))
  UnsafeWrapper("Unsafe_AllocateInstance");
  {
    ThreadToNativeFromVM ttnfv(thread);
    return env->AllocObject(cls);
  }
UNSAFE_END

UNSAFE_ENTRY(jlong, Unsafe_AllocateMemory(JNIEnv *env, jobject unsafe, jlong size))
  UnsafeWrapper("Unsafe_AllocateMemory");
  size_t sz = (size_t)size;
  if (sz != size || size < 0) {
    THROW_0(vmSymbols::java_lang_IllegalArgumentException());
  }
  if (sz == 0) {
    return 0;
  }
  sz = round_to(sz, HeapWordSize);
  void* x = os::malloc(sz);
  if (x == NULL) {
    THROW_0(vmSymbols::java_lang_OutOfMemoryError());
  }
  //Copy::fill_to_words((HeapWord*)x, sz / HeapWordSize);
  return addr_to_java(x);
UNSAFE_END

UNSAFE_ENTRY(jlong, Unsafe_ReallocateMemory(JNIEnv *env, jobject unsafe, jlong addr, jlong size))
  UnsafeWrapper("Unsafe_ReallocateMemory");
  void* p = addr_from_java(addr);
  size_t sz = (size_t)size;
  if (sz != size || size < 0) {
    THROW_0(vmSymbols::java_lang_IllegalArgumentException());
  }
  if (sz == 0) {
    os::free(p);
    return 0;
  }
  sz = round_to(sz, HeapWordSize);
  void* x = (p == NULL) ? os::malloc(sz) : os::realloc(p, sz);
  if (x == NULL) {
    THROW_0(vmSymbols::java_lang_OutOfMemoryError());
  }
  return addr_to_java(x);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_FreeMemory(JNIEnv *env, jobject unsafe, jlong addr))
  UnsafeWrapper("Unsafe_FreeMemory");
  void* p = addr_from_java(addr);
  if (p == NULL) {
    return;
  }
  os::free(p);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetMemory(JNIEnv *env, jobject unsafe, jlong addr, jlong size, jbyte value))
  UnsafeWrapper("Unsafe_SetMemory");
  size_t sz = (size_t)size;
  if (sz != size || size < 0) {
    THROW(vmSymbols::java_lang_IllegalArgumentException());
  }
  char* p = (char*) addr_from_java(addr);
  while ((uintptr_t)p % HeapWordSize && sz > 0) {
    *p++ = (char) value;
    sz--;
  }
  juint value_word = (juint)value & 0xFF;
  if (value_word != 0) {
    value_word |= (value_word << 8);
    value_word |= (value_word << 16);
  }
  size_t nw = sz / HeapWordSize;
  Copy::fill_to_words((HeapWord*)p, nw, value_word);
  sz -= nw * HeapWordSize;
  p  += nw * HeapWordSize;
  while (sz > 0) {
    *p++ = (char) value;
    sz--;
  }
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_CopyMemory(JNIEnv *env, jobject unsafe, jlong srcAddr, jlong dstAddr, jlong size))
  UnsafeWrapper("Unsafe_CopyMemory");
  if (size == 0) {
    return;
  }
  size_t sz = (size_t)size;
  if (sz != size || size < 0) {
    THROW(vmSymbols::java_lang_IllegalArgumentException());
  }
  if (sz == 0) {
    return;
  }
  void* src = addr_from_java(srcAddr);
  void* dst = addr_from_java(dstAddr);
  Copy::conjoint_bytes(src, dst, sz);
UNSAFE_END


////// Random queries

// See comment at file start about UNSAFE_LEAF
//UNSAFE_LEAF(jint, Unsafe_AddressSize())
UNSAFE_ENTRY(jint, Unsafe_AddressSize(JNIEnv *env, jobject unsafe))
  UnsafeWrapper("Unsafe_AddressSize");
  return sizeof(void*);
UNSAFE_END

// See comment at file start about UNSAFE_LEAF
//UNSAFE_LEAF(jint, Unsafe_PageSize())
UNSAFE_ENTRY(jint, Unsafe_PageSize(JNIEnv *env, jobject unsafe))
  UnsafeWrapper("Unsafe_PageSize");
  return os::vm_page_size();
UNSAFE_END

jint find_field_offset(jobject field, int must_be_static, TRAPS) {
  if (field == NULL) {
    THROW_0(vmSymbols::java_lang_NullPointerException());
  }

  oop reflected   = JNIHandles::resolve_non_null(field);
  oop mirror      = java_lang_reflect_Field::clazz(reflected);
  klassOop k      = java_lang_Class::as_klassOop(mirror);
  int slot        = java_lang_reflect_Field::slot(reflected);
  int modifiers   = java_lang_reflect_Field::modifiers(reflected);

  if (must_be_static >= 0) {
    int really_is_static = ((modifiers & JVM_ACC_STATIC) != 0);
    if (must_be_static != really_is_static) {
      THROW_0(vmSymbols::java_lang_IllegalArgumentException());
    }
  }

  int offset = instanceKlass::cast(k)->offset_from_fields(slot);
  return field_offset_from_byte_offset(offset);
}

UNSAFE_ENTRY(jlong, Unsafe_ObjectFieldOffset(JNIEnv *env, jobject unsafe, jobject field))
  UnsafeWrapper("Unsafe_ObjectFieldOffset");
  return find_field_offset(field, 0, THREAD);
UNSAFE_END

UNSAFE_ENTRY(jlong, Unsafe_StaticFieldOffset(JNIEnv *env, jobject unsafe, jobject field))
  UnsafeWrapper("Unsafe_StaticFieldOffset");
  return find_field_offset(field, 1, THREAD);
UNSAFE_END

UNSAFE_ENTRY(jobject, Unsafe_StaticFieldBaseFromField(JNIEnv *env, jobject unsafe, jobject field))
  UnsafeWrapper("Unsafe_StaticFieldBase");
  // Note:  In this VM implementation, a field address is always a short
  // offset from the base of a a klass metaobject.  Thus, the full dynamic
  // range of the return type is never used.  However, some implementations
  // might put the static field inside an array shared by many classes,
  // or even at a fixed address, in which case the address could be quite
  // large.  In that last case, this function would return NULL, since
  // the address would operate alone, without any base pointer.

  if (field == NULL)  THROW_0(vmSymbols::java_lang_NullPointerException());

  oop reflected   = JNIHandles::resolve_non_null(field);
  oop mirror      = java_lang_reflect_Field::clazz(reflected);
  int modifiers   = java_lang_reflect_Field::modifiers(reflected);

  if ((modifiers & JVM_ACC_STATIC) == 0) {
    THROW_0(vmSymbols::java_lang_IllegalArgumentException());
  }

  return JNIHandles::make_local(env, java_lang_Class::as_klassOop(mirror));
UNSAFE_END

//@deprecated
UNSAFE_ENTRY(jint, Unsafe_FieldOffset(JNIEnv *env, jobject unsafe, jobject field))
  UnsafeWrapper("Unsafe_FieldOffset");
  // tries (but fails) to be polymorphic between static and non-static:
  jlong offset = find_field_offset(field, -1, THREAD);
  guarantee(offset == (jint)offset, "offset fits in 32 bits");
  return (jint)offset;
UNSAFE_END

//@deprecated
UNSAFE_ENTRY(jobject, Unsafe_StaticFieldBaseFromClass(JNIEnv *env, jobject unsafe, jobject clazz))
  UnsafeWrapper("Unsafe_StaticFieldBase");
  if (clazz == NULL) {
    THROW_0(vmSymbols::java_lang_NullPointerException());
  }
  return JNIHandles::make_local(env, java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(clazz)));
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_EnsureClassInitialized(JNIEnv *env, jobject unsafe, jobject clazz))
  UnsafeWrapper("Unsafe_EnsureClassInitialized");
  if (clazz == NULL) {
    THROW(vmSymbols::java_lang_NullPointerException());
  }
  oop mirror = JNIHandles::resolve_non_null(clazz);
  instanceKlass* k = instanceKlass::cast(java_lang_Class::as_klassOop(mirror));
  if (k != NULL) {
    k->initialize(CHECK);
  }
UNSAFE_END

static void getBaseAndScale(int& base, int& scale, jclass acls, TRAPS) {
  if (acls == NULL) {
    THROW(vmSymbols::java_lang_NullPointerException());
  }
  oop      mirror = JNIHandles::resolve_non_null(acls);
  klassOop k      = java_lang_Class::as_klassOop(mirror);
  if (k == NULL || !k->klass_part()->oop_is_array()) {
    THROW(vmSymbols::java_lang_InvalidClassException());
  } else if (k->klass_part()->oop_is_objArray()) {
    base  = arrayOopDesc::base_offset_in_bytes(T_OBJECT);
    scale = oopSize;
  } else if (k->klass_part()->oop_is_typeArray()) {
    typeArrayKlass* tak = typeArrayKlass::cast(k);
    base  = tak->array_header_in_bytes();
    assert(base == arrayOopDesc::base_offset_in_bytes(tak->type()), "array_header_size semantics ok");
    scale = tak->scale();
  } else {
    ShouldNotReachHere();
  }
}

UNSAFE_ENTRY(jint, Unsafe_ArrayBaseOffset(JNIEnv *env, jobject unsafe, jclass acls))
  UnsafeWrapper("Unsafe_ArrayBaseOffset");
  int base, scale;
  getBaseAndScale(base, scale, acls, CHECK_0);
  return field_offset_from_byte_offset(base);
UNSAFE_END


UNSAFE_ENTRY(jint, Unsafe_ArrayIndexScale(JNIEnv *env, jobject unsafe, jclass acls))
  UnsafeWrapper("Unsafe_ArrayIndexScale");
  int base, scale;
  getBaseAndScale(base, scale, acls, CHECK_0);
  // This VM packs both fields and array elements down to the byte.
  // But watch out:  If this changes, so that array references for
  // a given primitive type (say, T_BOOLEAN) use different memory units
  // than fields, this method MUST return zero for such arrays.
  // For example, the VM used to store sub-word sized fields in full
  // words in the object layout, so that accessors like getByte(Object,int)
  // did not really do what one might expect for arrays.  Therefore,
  // this function used to report a zero scale factor, so that the user
  // would know not to attempt to access sub-word array elements.
  // // Code for unpacked fields:
  // if (scale < wordSize)  return 0;

  // The following allows for a pretty general fieldOffset cookie scheme,
  // but requires it to be linear in byte offset.
  return field_offset_from_byte_offset(scale) - field_offset_from_byte_offset(0);
UNSAFE_END


static inline void throw_new(JNIEnv *env, const char *ename) {
  char buf[100];
  strcpy(buf, "java/lang/");
  strcat(buf, ename);
  jclass cls = env->FindClass(buf);
  char* msg = NULL;
  env->ThrowNew(cls, msg);
}

static jclass Unsafe_DefineClass(JNIEnv *env, jstring name, jbyteArray data, int offset, int length, jobject loader, jobject pd) {
  {
    // Code lifted from JDK 1.3 ClassLoader.c

    jbyte *body;
    char *utfName;
    jclass result = 0;
    char buf[128];

    if (data == NULL) {
	throw_new(env, "NullPointerException");
	return 0;
    }

    /* Work around 4153825. malloc crashes on Solaris when passed a
     * negative size.
     */
    if (length < 0) {
        throw_new(env, "ArrayIndexOutOfBoundsException");
	return 0;
    }

    body = NEW_C_HEAP_ARRAY(jbyte, length);

    if (body == 0) {
        throw_new(env, "OutOfMemoryError");
	return 0;
    }

    env->GetByteArrayRegion(data, offset, length, body);

    if (env->ExceptionOccurred())
        goto free_body;

    if (name != NULL) {
        int len = env->GetStringUTFLength(name);
	int unicode_len = env->GetStringLength(name);
        if (len >= sizeof(buf)) {
            utfName = NEW_C_HEAP_ARRAY(char, len + 1);
            if (utfName == NULL) {
                throw_new(env, "OutOfMemoryError");
                goto free_body;
            }
        } else {
            utfName = buf;
        }
    	env->GetStringUTFRegion(name, 0, unicode_len, utfName);
	//VerifyFixClassname(utfName);
	for (int i = 0; i < len; i++) {
	  if (utfName[i] == '.')   utfName[i] = '/';
	}
    } else {
	utfName = NULL;
    }

    result = JVM_DefineClass(env, utfName, loader, body, length, pd);

    if (utfName && utfName != buf) 
        FREE_C_HEAP_ARRAY(char, utfName);

 free_body:
    FREE_C_HEAP_ARRAY(jbyte, body);
    return result;
  }
}


UNSAFE_ENTRY(jclass, Unsafe_DefineClass0(JNIEnv *env, jobject unsafe, jstring name, jbyteArray data, int offset, int length))
  UnsafeWrapper("Unsafe_DefineClass");
  {
    ThreadToNativeFromVM ttnfv(thread);

    int depthFromDefineClass0 = 1;
    jclass  caller = JVM_GetCallerClass(env, depthFromDefineClass0);
    jobject loader = (caller == NULL) ? NULL : JVM_GetClassLoader(env, caller);
    jobject pd     = (caller == NULL) ? NULL : JVM_GetProtectionDomain(env, caller);

    return Unsafe_DefineClass(env, name, data, offset, length, loader, pd);
  }
UNSAFE_END


UNSAFE_ENTRY(jclass, Unsafe_DefineClass1(JNIEnv *env, jobject unsafe, jstring name, jbyteArray data, int offset, int length, jobject loader, jobject pd))
  UnsafeWrapper("Unsafe_DefineClass");
  {
    ThreadToNativeFromVM ttnfv(thread);

    return Unsafe_DefineClass(env, name, data, offset, length, loader, pd);
  }
UNSAFE_END


UNSAFE_ENTRY(void, Unsafe_MonitorEnter(JNIEnv *env, jobject unsafe, jobject obj))
  UnsafeWrapper("Unsafe_MonitorEnter");
  {
    ThreadToNativeFromVM ttnfv(thread);
    env->MonitorEnter(obj);
  }
UNSAFE_END


UNSAFE_ENTRY(void, Unsafe_MonitorExit(JNIEnv *env, jobject unsafe, jobject obj))
  UnsafeWrapper("Unsafe_MonitorExit");
  {
    ThreadToNativeFromVM ttnfv(thread);
    env->MonitorExit(obj);
  }
UNSAFE_END


UNSAFE_ENTRY(void, Unsafe_ThrowException(JNIEnv *env, jobject unsafe, jthrowable thr))
  UnsafeWrapper("Unsafe_ThrowException");
  {
    ThreadToNativeFromVM ttnfv(thread);
    env->Throw(thr);
  }
UNSAFE_END

// JSR166 ------------------------------------------------------------------

UNSAFE_ENTRY(jboolean, Unsafe_CompareAndSwapObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject e_h, jobject x_h))
  UnsafeWrapper("Unsafe_CompareAndSwapObject");
  oop x = JNIHandles::resolve(x_h); 
  oop e = JNIHandles::resolve(e_h); 
  oop p = JNIHandles::resolve(obj);
  intptr_t* addr = (intptr_t *)index_oop_from_field_offset_long(p, offset);
  intptr_t res = Atomic::cmpxchg_ptr((intptr_t)x, addr, (intptr_t)e);
  jboolean success  = (res == (intptr_t)e);
  if (success)
    update_barrier_set((oop*)addr, x);
  return success;
UNSAFE_END

UNSAFE_ENTRY(jboolean, Unsafe_CompareAndSwapInt(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jint e, jint x))
  UnsafeWrapper("Unsafe_CompareAndSwapInt");
  oop p = JNIHandles::resolve(obj);
  jint* addr = (jint *) index_oop_from_field_offset_long(p, offset);
  return (jint)(Atomic::cmpxchg(x, addr, e)) == e;
UNSAFE_END

UNSAFE_ENTRY(jboolean, Unsafe_CompareAndSwapLong(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong e, jlong x))
  UnsafeWrapper("Unsafe_CompareAndSwapLong");
  oop p = JNIHandles::resolve(obj);
  jlong* addr = (jlong*)(index_oop_from_field_offset_long(p, offset));
  if (VM_Version::supports_cx8())
    return (jlong)(Atomic::cmpxchg(x, addr, e)) == e;
  else {
    jboolean success = false;
    ObjectLocker ol(p, THREAD);
    if (*addr == e) { *addr = x; success = true; }
    return success;
  }
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_Park(JNIEnv *env, jobject unsafe, jboolean isAbsolute, jlong time))
  UnsafeWrapper("Unsafe_Park");
  JavaThreadParkedState jtps(thread, time != 0);
  thread->parker()->park(isAbsolute, time);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_Unpark(JNIEnv *env, jobject unsafe, jobject jthread))
  UnsafeWrapper("Unsafe_Unpark");
  // Be careful about trying to unpark non-existent threads.  But if we
  // can resolve thread and get ptr to parker now, we know it will not
  // disappear before internal unpark because Java caller still holds
  // ref to it.
  if (jthread != NULL) {
    oop java_thread = JNIHandles::resolve_non_null(jthread);
    if (java_thread != NULL) {
      JavaThread* thr = java_lang_Thread::thread(java_thread);
      if (thr != NULL) {
        Parker* p = thr->parker();
        if (p != NULL) {
          p->unpark();
        }
      }
    }
  }
UNSAFE_END



/// JVM_RegisterUnsafeMethods

#define ADR "J"

#define LANG "Ljava/lang/"

#define OBJ LANG"Object;"
#define CLS LANG"Class;"
#define CTR LANG"reflect/Constructor;"
#define FLD LANG"reflect/Field;"
#define MTH LANG"reflect/Method;"
#define THR LANG"Throwable;"

#define DC0_Args LANG"String;[BII"
#define DC1_Args DC0_Args LANG"ClassLoader;" "Ljava/security/ProtectionDomain;"

#define CC (char*)  /*cast a literal from (const char*)*/
#define FN_PTR(f) CAST_FROM_FN_PTR(void*, &f)

// define deprecated accessors for compabitility with 1.4.0
#define DECLARE_GETSETOOP_140(Boolean, Z) \
    {CC"get"#Boolean,      CC"("OBJ"I)"#Z,      FN_PTR(Unsafe_Get##Boolean##140)}, \
    {CC"put"#Boolean,      CC"("OBJ"I"#Z")V",   FN_PTR(Unsafe_Set##Boolean##140)}

// Note:  In 1.4.1, getObject and kin take both int and long offsets.
#define DECLARE_GETSETOOP_141(Boolean, Z) \
    {CC"get"#Boolean,      CC"("OBJ"J)"#Z,      FN_PTR(Unsafe_Get##Boolean)}, \
    {CC"put"#Boolean,      CC"("OBJ"J"#Z")V",   FN_PTR(Unsafe_Set##Boolean)}

// Note:  In 1.5.0, there are volatile versions too
#define DECLARE_GETSETOOP(Boolean, Z) \
    {CC"get"#Boolean,      CC"("OBJ"J)"#Z,      FN_PTR(Unsafe_Get##Boolean)}, \
    {CC"put"#Boolean,      CC"("OBJ"J"#Z")V",   FN_PTR(Unsafe_Set##Boolean)}, \
    {CC"get"#Boolean"Volatile",      CC"("OBJ"J)"#Z,      FN_PTR(Unsafe_Get##Boolean##Volatile)}, \
    {CC"put"#Boolean"Volatile",      CC"("OBJ"J"#Z")V",   FN_PTR(Unsafe_Set##Boolean##Volatile)}


#define DECLARE_GETSETNATIVE(Byte, B) \
    {CC"get"#Byte,         CC"("ADR")"#B,       FN_PTR(Unsafe_GetNative##Byte)}, \
    {CC"put"#Byte,         CC"("ADR#B")V",      FN_PTR(Unsafe_SetNative##Byte)}



// %%% These are temporarily supported until the SDK sources
// contain the necessarily updated Unsafe.java.
static JNINativeMethod methods_140[] = {

    {CC"getObject",        CC"("OBJ"I)"OBJ"",   FN_PTR(Unsafe_GetObject140)},
    {CC"putObject",        CC"("OBJ"I"OBJ")V",  FN_PTR(Unsafe_SetObject140)},

    DECLARE_GETSETOOP_140(Boolean, Z),
    DECLARE_GETSETOOP_140(Byte, B),
    DECLARE_GETSETOOP_140(Short, S),
    DECLARE_GETSETOOP_140(Char, C),
    DECLARE_GETSETOOP_140(Int, I),
    DECLARE_GETSETOOP_140(Long, J),
    DECLARE_GETSETOOP_140(Float, F),
    DECLARE_GETSETOOP_140(Double, D),

    DECLARE_GETSETNATIVE(Byte, B),
    DECLARE_GETSETNATIVE(Short, S),
    DECLARE_GETSETNATIVE(Char, C),
    DECLARE_GETSETNATIVE(Int, I),
    DECLARE_GETSETNATIVE(Long, J),
    DECLARE_GETSETNATIVE(Float, F),
    DECLARE_GETSETNATIVE(Double, D),

    {CC"getAddress",         CC"("ADR")"ADR,             FN_PTR(Unsafe_GetNativeAddress)},
    {CC"putAddress",         CC"("ADR""ADR")V",          FN_PTR(Unsafe_SetNativeAddress)},

    {CC"allocateMemory",     CC"(J)"ADR,                 FN_PTR(Unsafe_AllocateMemory)},
    {CC"reallocateMemory",   CC"("ADR"J)"ADR,            FN_PTR(Unsafe_ReallocateMemory)},
    {CC"setMemory",          CC"("ADR"JB)V",             FN_PTR(Unsafe_SetMemory)},
    {CC"copyMemory",         CC"("ADR ADR"J)V",          FN_PTR(Unsafe_CopyMemory)},
    {CC"freeMemory",         CC"("ADR")V",               FN_PTR(Unsafe_FreeMemory)},

    {CC"fieldOffset",        CC"("FLD")I",               FN_PTR(Unsafe_FieldOffset)}, //deprecated
    {CC"staticFieldBase",    CC"("CLS")"OBJ,             FN_PTR(Unsafe_StaticFieldBaseFromClass)}, //deprecated
    {CC"ensureClassInitialized",CC"("CLS")V",            FN_PTR(Unsafe_EnsureClassInitialized)},
    {CC"arrayBaseOffset",    CC"("CLS")I",               FN_PTR(Unsafe_ArrayBaseOffset)},
    {CC"arrayIndexScale",    CC"("CLS")I",               FN_PTR(Unsafe_ArrayIndexScale)},
    {CC"addressSize",        CC"()I",                    FN_PTR(Unsafe_AddressSize)},
    {CC"pageSize",           CC"()I",                    FN_PTR(Unsafe_PageSize)},

    {CC"defineClass",        CC"("DC0_Args")"CLS,        FN_PTR(Unsafe_DefineClass0)},
    {CC"defineClass",        CC"("DC1_Args")"CLS,        FN_PTR(Unsafe_DefineClass1)},
    {CC"allocateInstance",   CC"("CLS")"OBJ,             FN_PTR(Unsafe_AllocateInstance)},
    {CC"monitorEnter",       CC"("OBJ")V",               FN_PTR(Unsafe_MonitorEnter)},
    {CC"monitorExit",        CC"("OBJ")V",               FN_PTR(Unsafe_MonitorExit)},
    {CC"throwException",     CC"("THR")V",               FN_PTR(Unsafe_ThrowException)}
};

// These are the old methods prior to the JSR 166 changes in 1.5.0
static JNINativeMethod methods_141[] = {

    {CC"getObject",        CC"("OBJ"J)"OBJ"",   FN_PTR(Unsafe_GetObject)},
    {CC"putObject",        CC"("OBJ"J"OBJ")V",  FN_PTR(Unsafe_SetObject)},

    DECLARE_GETSETOOP_141(Boolean, Z),
    DECLARE_GETSETOOP_141(Byte, B),
    DECLARE_GETSETOOP_141(Short, S),
    DECLARE_GETSETOOP_141(Char, C),
    DECLARE_GETSETOOP_141(Int, I),
    DECLARE_GETSETOOP_141(Long, J),
    DECLARE_GETSETOOP_141(Float, F),
    DECLARE_GETSETOOP_141(Double, D),

    DECLARE_GETSETNATIVE(Byte, B),
    DECLARE_GETSETNATIVE(Short, S),
    DECLARE_GETSETNATIVE(Char, C),
    DECLARE_GETSETNATIVE(Int, I),
    DECLARE_GETSETNATIVE(Long, J),
    DECLARE_GETSETNATIVE(Float, F),
    DECLARE_GETSETNATIVE(Double, D),

    {CC"getAddress",         CC"("ADR")"ADR,             FN_PTR(Unsafe_GetNativeAddress)},
    {CC"putAddress",         CC"("ADR""ADR")V",          FN_PTR(Unsafe_SetNativeAddress)},

    {CC"allocateMemory",     CC"(J)"ADR,                 FN_PTR(Unsafe_AllocateMemory)},
    {CC"reallocateMemory",   CC"("ADR"J)"ADR,            FN_PTR(Unsafe_ReallocateMemory)},
    {CC"setMemory",          CC"("ADR"JB)V",             FN_PTR(Unsafe_SetMemory)},
    {CC"copyMemory",         CC"("ADR ADR"J)V",          FN_PTR(Unsafe_CopyMemory)},
    {CC"freeMemory",         CC"("ADR")V",               FN_PTR(Unsafe_FreeMemory)},

    {CC"objectFieldOffset",  CC"("FLD")J",               FN_PTR(Unsafe_ObjectFieldOffset)},
    {CC"staticFieldOffset",  CC"("FLD")J",               FN_PTR(Unsafe_StaticFieldOffset)},
    {CC"staticFieldBase",    CC"("FLD")"OBJ,             FN_PTR(Unsafe_StaticFieldBaseFromField)},
    {CC"ensureClassInitialized",CC"("CLS")V",            FN_PTR(Unsafe_EnsureClassInitialized)},
    {CC"arrayBaseOffset",    CC"("CLS")I",               FN_PTR(Unsafe_ArrayBaseOffset)},
    {CC"arrayIndexScale",    CC"("CLS")I",               FN_PTR(Unsafe_ArrayIndexScale)},
    {CC"addressSize",        CC"()I",                    FN_PTR(Unsafe_AddressSize)},
    {CC"pageSize",           CC"()I",                    FN_PTR(Unsafe_PageSize)},

    {CC"defineClass",        CC"("DC0_Args")"CLS,        FN_PTR(Unsafe_DefineClass0)},
    {CC"defineClass",        CC"("DC1_Args")"CLS,        FN_PTR(Unsafe_DefineClass1)},
    {CC"allocateInstance",   CC"("CLS")"OBJ,             FN_PTR(Unsafe_AllocateInstance)},
    {CC"monitorEnter",       CC"("OBJ")V",               FN_PTR(Unsafe_MonitorEnter)},
    {CC"monitorExit",        CC"("OBJ")V",               FN_PTR(Unsafe_MonitorExit)},
    {CC"throwException",     CC"("THR")V",               FN_PTR(Unsafe_ThrowException)}

};

// These are the correct methods, moving forward:
static JNINativeMethod methods[] = {

    {CC"getObject",        CC"("OBJ"J)"OBJ"",   FN_PTR(Unsafe_GetObject)},
    {CC"putObject",        CC"("OBJ"J"OBJ")V",  FN_PTR(Unsafe_SetObject)},
    {CC"getObjectVolatile",CC"("OBJ"J)"OBJ"",   FN_PTR(Unsafe_GetObjectVolatile)},
    {CC"putObjectVolatile",CC"("OBJ"J"OBJ")V",  FN_PTR(Unsafe_SetObjectVolatile)},


    DECLARE_GETSETOOP(Boolean, Z),
    DECLARE_GETSETOOP(Byte, B),
    DECLARE_GETSETOOP(Short, S),
    DECLARE_GETSETOOP(Char, C),
    DECLARE_GETSETOOP(Int, I),
    DECLARE_GETSETOOP(Long, J),
    DECLARE_GETSETOOP(Float, F),
    DECLARE_GETSETOOP(Double, D),

    DECLARE_GETSETNATIVE(Byte, B),
    DECLARE_GETSETNATIVE(Short, S),
    DECLARE_GETSETNATIVE(Char, C),
    DECLARE_GETSETNATIVE(Int, I),
    DECLARE_GETSETNATIVE(Long, J),
    DECLARE_GETSETNATIVE(Float, F),
    DECLARE_GETSETNATIVE(Double, D),

    {CC"getAddress",         CC"("ADR")"ADR,             FN_PTR(Unsafe_GetNativeAddress)},
    {CC"putAddress",         CC"("ADR""ADR")V",          FN_PTR(Unsafe_SetNativeAddress)},

    {CC"allocateMemory",     CC"(J)"ADR,                 FN_PTR(Unsafe_AllocateMemory)},
    {CC"reallocateMemory",   CC"("ADR"J)"ADR,            FN_PTR(Unsafe_ReallocateMemory)},
    {CC"setMemory",          CC"("ADR"JB)V",             FN_PTR(Unsafe_SetMemory)},
    {CC"copyMemory",         CC"("ADR ADR"J)V",          FN_PTR(Unsafe_CopyMemory)},
    {CC"freeMemory",         CC"("ADR")V",               FN_PTR(Unsafe_FreeMemory)},

    {CC"objectFieldOffset",  CC"("FLD")J",               FN_PTR(Unsafe_ObjectFieldOffset)},
    {CC"staticFieldOffset",  CC"("FLD")J",               FN_PTR(Unsafe_StaticFieldOffset)},
    {CC"staticFieldBase",    CC"("FLD")"OBJ,             FN_PTR(Unsafe_StaticFieldBaseFromField)},
    {CC"ensureClassInitialized",CC"("CLS")V",            FN_PTR(Unsafe_EnsureClassInitialized)},
    {CC"arrayBaseOffset",    CC"("CLS")I",               FN_PTR(Unsafe_ArrayBaseOffset)},
    {CC"arrayIndexScale",    CC"("CLS")I",               FN_PTR(Unsafe_ArrayIndexScale)},
    {CC"addressSize",        CC"()I",                    FN_PTR(Unsafe_AddressSize)},
    {CC"pageSize",           CC"()I",                    FN_PTR(Unsafe_PageSize)},

    {CC"defineClass",        CC"("DC0_Args")"CLS,        FN_PTR(Unsafe_DefineClass0)},
    {CC"defineClass",        CC"("DC1_Args")"CLS,        FN_PTR(Unsafe_DefineClass1)},
    {CC"allocateInstance",   CC"("CLS")"OBJ,             FN_PTR(Unsafe_AllocateInstance)},
    {CC"monitorEnter",       CC"("OBJ")V",               FN_PTR(Unsafe_MonitorEnter)},
    {CC"monitorExit",        CC"("OBJ")V",               FN_PTR(Unsafe_MonitorExit)},
    {CC"throwException",     CC"("THR")V",               FN_PTR(Unsafe_ThrowException)},
    {CC"compareAndSwapObject", CC"("OBJ"J"OBJ""OBJ")Z",  FN_PTR(Unsafe_CompareAndSwapObject)},
    {CC"compareAndSwapInt",    CC"("OBJ"J""I""I"")Z",    FN_PTR(Unsafe_CompareAndSwapInt)},
    {CC"compareAndSwapLong",   CC"("OBJ"J""J""J"")Z",    FN_PTR(Unsafe_CompareAndSwapLong)},
    {CC"park",               CC"(ZJ)V",                  FN_PTR(Unsafe_Park)},
    {CC"unpark",             CC"("OBJ")V",               FN_PTR(Unsafe_Unpark)}

};

#undef CC
#undef FN_PTR

#undef ADR
#undef LANG
#undef OBJ
#undef CLS
#undef CTR
#undef FLD
#undef MTH
#undef THR
#undef DC0_Args
#undef DC1_Args

#undef DECLARE_GETSETOOP
#undef DECLARE_GETSETNATIVE


// This one function is exported, used by NativeLookup.
// The Unsafe_xxx functions above are called only from the interpreter.
// The optimizer looks at names and signatures to recognize
// individual functions.

JVM_ENTRY(void, JVM_RegisterUnsafeMethods(JNIEnv *env, jclass unsafecls))
  UnsafeWrapper("JVM_RegisterUnsafeMethods");
  {
    ThreadToNativeFromVM ttnfv(thread);
    bool ok = env->RegisterNatives(unsafecls, methods, sizeof(methods)/sizeof(JNINativeMethod));
    if (env->ExceptionOccurred()) {
      if (PrintMiscellaneous && (Verbose || WizardMode)) {
        tty->print_cr("Warning:  SDK 1.5 version of Unsafe not found.");
      }
      env->ExceptionClear();
      // %%% For now, be backward compatible with an older class:
      ok = env->RegisterNatives(unsafecls, methods_141, sizeof(methods_141)/sizeof(JNINativeMethod));
    }
    if (env->ExceptionOccurred()) {
      if (PrintMiscellaneous && (Verbose || WizardMode)) {
        tty->print_cr("Warning:  SDK 1.4.1 version of Unsafe not found.");
      }
      env->ExceptionClear();
      // %%% For now, be backward compatible with an older class:
      ok = env->RegisterNatives(unsafecls, methods_140, sizeof(methods_140)/sizeof(JNINativeMethod));
    }
    guarantee(ok == 0, "register unsafe natives");
  }
JVM_END
