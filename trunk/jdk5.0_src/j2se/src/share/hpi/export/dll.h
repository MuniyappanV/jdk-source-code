/*
 * @(#)dll.h	1.8 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#ifndef _JAVASOFT_DLL_H_
#define _JAVASOFT_DLL_H_

#include <jni.h>

/* DLL.H: A common interface for helper DLLs loaded by the VM.
 * Each library exports the main entry point "DLL_Initialize". Through
 * that function the programmer can obtain a function pointer which has
 * type "GetInterfaceFunc." Through the function pointer the programmer
 * can obtain other interfaces supported in the DLL.
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef jint (JNICALL * GetInterfaceFunc)
       (void **intfP, const char *name, jint ver);

jint JNICALL DLL_Initialize(GetInterfaceFunc *, void *args);

#ifdef __cplusplus
}
#endif

#endif /* !_JAVASOFT_DLL_H_ */
