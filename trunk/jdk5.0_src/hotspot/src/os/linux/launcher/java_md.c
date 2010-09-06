#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)java_md.c	1.5 03/12/23 16:37:28 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "java.h"
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#ifdef DEBUG
#define JVM_DLL "libjvm_g.so"
#else
#define JVM_DLL "libjvm.so"
#endif

/*
 * Load JVM of "jvmtype", and intialize the invocation functions.  On
 * Linux, currently, this is a no-op, and we always use classic VM.
 */
jboolean
LoadJavaVM(char *jvmtype, InvocationFunctions *ifn)
{
    ifn->CreateJavaVM = JNI_CreateJavaVM;
    ifn->GetDefaultJavaVMInitArgs = JNI_GetDefaultJavaVMInitArgs;
    return JNI_TRUE;
}

/*
 * Get the path to the file that has the usage message for -X options.
 */
void
GetXUsagePath(char *buf, jint bufsize)
{
    Dl_info dlinfo;
   
    /* we use RTLD_NOW because of problems with ld.so.1 and green threads */
    dladdr(dlsym(dlopen(JVM_DLL, RTLD_NOW), "JNI_CreateJavaVM"), &dlinfo);
    strncpy(buf, (char *)dlinfo.dli_fname, bufsize - 1);
    buf[bufsize - 1] = '\0';
    *(strrchr(buf, '/')) = '\0';
    strcat(buf, "/Xusage.txt");
}

/*
 * If app is "/foo/bin/sparc/green_threads/javac", then put "/foo" into buf.
 */
jboolean
GetApplicationHome(char *buf, jint bufsize)
{
#ifdef USE_APPHOME
    char *apphome = getenv("APPHOME");
    if (apphome) {
	strncpy(buf, apphome, bufsize-1);
	buf[bufsize - 1] = '\0';
	return JNI_TRUE;
    } else {
	return JNI_FALSE;
    }
#else
    Dl_info dlinfo;

    dladdr((void *)GetApplicationHome, &dlinfo);
    strncpy(buf, dlinfo.dli_fname, bufsize - 1);
    buf[bufsize - 1] = '\0';
    
    *(strrchr(buf, '/')) = '\0';  /* executable file      */
    *(strrchr(buf, '/')) = '\0';  /* green|native_threads */
    *(strrchr(buf, '/')) = '\0';  /* sparc|i386           */
    *(strrchr(buf, '/')) = '\0';  /* bin                  */
    return JNI_TRUE;
#endif
}
