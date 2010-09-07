/*
 * Copyright (c) 1998, 2009, Oracle and/or its affiliates. All rights reserved.
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

// Useful entry points shared by JNI and JVM interface.
// We do not allow real JNI or JVM entry point to call each other.

jclass find_class_from_class_loader(JNIEnv* env, symbolHandle name, jboolean init, Handle loader, Handle protection_domain, jboolean throwError, TRAPS);

void trace_class_resolution(klassOop to_class);

/*
 * Support for Serialization and RMI. Currently used by HotSpot only.
 */

extern "C" {

void JNICALL
JVM_SetPrimitiveFieldValues(JNIEnv *env, jclass cb, jobject obj,
                            jlongArray fieldIDs, jcharArray typecodes, jbyteArray data);

void JNICALL
JVM_GetPrimitiveFieldValues(JNIEnv *env, jclass cb, jobject obj,
                            jlongArray fieldIDs, jcharArray typecodes, jbyteArray data);

}

/*
 * Support for -Xcheck:jni
 */

extern struct JNINativeInterface_* jni_functions_nocheck();
extern struct JNINativeInterface_* jni_functions_check();

/*
 * Support for swappable jni function table.
 */
extern struct JNINativeInterface_* jni_functions();
extern void copy_jni_function_table(const struct JNINativeInterface_* new_function_table);

// Support for fast JNI accessors
extern "C" {
  typedef jboolean (JNICALL *GetBooleanField_t)
      (JNIEnv *env, jobject obj, jfieldID fieldID);
  typedef jbyte (JNICALL *GetByteField_t)
      (JNIEnv *env, jobject obj, jfieldID fieldID);
  typedef jchar (JNICALL *GetCharField_t)
      (JNIEnv *env, jobject obj, jfieldID fieldID);
  typedef jshort (JNICALL *GetShortField_t)
      (JNIEnv *env, jobject obj, jfieldID fieldID);
  typedef jint (JNICALL *GetIntField_t)
      (JNIEnv *env, jobject obj, jfieldID fieldID);
  typedef jlong (JNICALL *GetLongField_t)
      (JNIEnv *env, jobject obj, jfieldID fieldID);
  typedef jfloat (JNICALL *GetFloatField_t)
      (JNIEnv *env, jobject obj, jfieldID fieldID);
  typedef jdouble (JNICALL *GetDoubleField_t)
    (JNIEnv *env, jobject obj, jfieldID fieldID);
}

void    quicken_jni_functions();
address jni_GetBooleanField_addr();
address jni_GetByteField_addr();
address jni_GetCharField_addr();
address jni_GetShortField_addr();
address jni_GetIntField_addr();
address jni_GetLongField_addr();
address jni_GetFloatField_addr();
address jni_GetDoubleField_addr();
