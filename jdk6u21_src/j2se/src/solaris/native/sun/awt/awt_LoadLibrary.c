/*
 * @(#)awt_LoadLibrary.c	1.32 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <jni.h>
#include <jni_util.h>
#include <jvm.h>
#include "gdefs.h"

#include <sys/param.h>
#include <sys/utsname.h>

#include "awt_Plugin.h"

#ifdef DEBUG
#define VERBOSE_AWT_DEBUG
#endif

static void *awtHandle = NULL;

typedef JNIEXPORT jint JNICALL JNI_OnLoad_type(JavaVM *vm, void *reserved);

/* Initialize the Java VM instance variable when the library is
   first loaded */
JavaVM *jvm;

JNIEXPORT jboolean JNICALL AWTIsHeadless() {
    static JNIEnv *env = NULL;
    static jboolean isHeadless;
    jmethodID headlessFn;
    jclass graphicsEnvClass;

    if (env == NULL) {
        env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
        graphicsEnvClass = (*env)->FindClass(env,
                                             "java/awt/GraphicsEnvironment");
        if (graphicsEnvClass == NULL) {
            return JNI_TRUE;
        }
        headlessFn = (*env)->GetStaticMethodID(env,
                                               graphicsEnvClass, "isHeadless", "()Z");
        if (headlessFn == NULL) {
            return JNI_TRUE;
        }
        isHeadless = (*env)->CallStaticBooleanMethod(env, graphicsEnvClass,
                                                     headlessFn);
    }
    return isHeadless;
}

jint 
AWT_OnLoad(JavaVM *vm, void *reserved)
{
    Dl_info dlinfo;
    char buf[MAXPATHLEN];
    int32_t len;
    char *p;
    JNI_OnLoad_type *JNI_OnLoad_ptr;
    int32_t motifVersion = 2;
    struct utsname name; 
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(vm, JNI_VERSION_1_2);
    void *v;
    char *envvar;
    int xt_before_xm = 0;
    int XAWT = 0;
    jstring toolkit = NULL;
    jstring propname = NULL;

    if (awtHandle != NULL) {
        /* Avoid several loading attempts */
        return JNI_VERSION_1_2;
    }

    jvm = vm;

    /* Get address of this library and the directory containing it. */
    dladdr((void *)JNI_OnLoad, &dlinfo);
    realpath((char *)dlinfo.dli_fname, buf);
    len = strlen(buf);
    p = strrchr(buf, '/');

    /*
     * The code below is responsible for:
     * 1. Loading appropriate awt library, i.e. motif/libmawt, xawt/libmawt or headless/libwawt
     * 2. Setting "awt.toolkit" system property to use the appropriate Java toolkit class,
     *    (if user has specified the toolkit in env varialble)
     */

    propname = (*env)->NewStringUTF(env, "awt.toolkit");
    /* Check if toolkit is specified in env variable */
    envvar = getenv("AWT_TOOLKIT"); 
    if (envvar) {
        if (strstr(envvar, "MToolkit")) {
            toolkit = (*env)->NewStringUTF(env, "sun.awt.motif.MToolkit");
        }
        else if (strstr(envvar, "XToolkit")) {
            toolkit = (*env)->NewStringUTF(env, "sun.awt.X11.XToolkit");
        }
        /* If user specified toolkit then set java system property */
        if (toolkit && propname) {
            JNU_CallStaticMethodByName (env,
                                        NULL,
                                        "java/lang/System",
                                        "setProperty",
                                        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                                        propname,toolkit);
        }
    }
    
    /* Calculate toolkit name, kind of toolkit (XAWT, Motif) and library to load */
    if (AWTIsHeadless()) {
        strcpy(p, "/headless/libmawt");
    } else {
        /* Try java system property */
        if (!toolkit && propname) {
            toolkit = JNU_CallStaticMethodByName (env,
                                                  NULL,
                                                  "java/lang/System",
                                                  "getProperty",
                                                  "(Ljava/lang/String;)Ljava/lang/String;",
                                                  propname).l;
        }

        /* Calculate kind of toolkit */
        if (toolkit) {
            const char* toolkit_name = (*env)->GetStringUTFChars(env, toolkit, 0);
            if (strstr(toolkit_name,"MToolkit")) {
                XAWT = 0;
            } else {
                XAWT = 1;
            }
            if (toolkit_name) {
                (*env)->ReleaseStringUTFChars(env, toolkit, toolkit_name);
            }
        } else {
            /* Default AWT Toolkit on Linux and Solaris is XAWT. */
            XAWT = 1;
        }

        /* Find loaded Motif version and if Xt is loaded */
        if (!XAWT) {
            /* Has a Motif library been loaded already, e.g. by an application
             * embedding java?   Netscape plugin?
             */

            v = dlsym(RTLD_DEFAULT, "vendorShellWidgetClass");
            if (v != NULL && dladdr(v, &dlinfo)) {

                /*
                 * If we are picking up the vendorShellWigetClass from libXt
                 * instead of libXm (it exists in both), then we are very stuck.
                 * Abort.
                 */
                if (strstr(dlinfo.dli_fname, "libXt.so") != NULL) {
                    xt_before_xm = 1;
                }

                if (strstr(dlinfo.dli_fname, "libXm.so.3") != NULL) {
#ifdef VERBOSE_AWT_DEBUG
                    fprintf(stderr, "Motif 1.2 detected, using that.\n");
#endif
                    motifVersion = 1;
                }
                else if (strstr(dlinfo.dli_fname, "libXm.so.4") != NULL) {
#ifdef VERBOSE_AWT_DEBUG
                    fprintf(stderr, "Motif 2.1 detected, using that.\n");
#endif
                    motifVersion = 2;
                }
            }

            /* Determine desired Motif Version, and set appropriate properties
             * to load the correct version of libmawt.so
             */  
            else {  
                uname(&name); 
    
                if ((strcmp(name.release, "5.5.1") == 0) ||
                    (strcmp(name.release, "5.6") == 0)) {
#ifdef VERBOSE_AWT_DEBUG
                    fprintf(stderr, "default to Motif 1.2, os is: %s\n",name.release);
#endif
                    motifVersion = 1;
                } else {
#ifdef VERBOSE_AWT_DEBUG
                    fprintf(stderr, "default to Motif 2.1, os is: %s\n",name.release);
#endif
                    motifVersion = 2;
                }
                if (getenv("_JAVA_AWT_USE_MOTIF_1_2")) {
#ifdef VERBOSE_AWT_DEBUG
                    fprintf(stderr,"_JAVA_AWT_USE_MOTIF_1_2 is set, using Motif 1.2\n");
#endif
                    motifVersion = 1;
                } else if (getenv("_JAVA_AWT_USE_MOTIF_2_1")) {
#ifdef VERBOSE_AWT_DEBUG
                    fprintf(stderr,"_JAVA_AWT_USE_MOTIF_2_1 is set, using Motif 2.1\n");
#endif
                    motifVersion = 2;
                }
            }
        }
    
        /* Update library name */
        if (toolkit) {
            const char* toolkit_name = (*env)->GetStringUTFChars(env, toolkit, 0);
            if (strstr(toolkit_name,"MToolkit")) {
                strcpy(p, (motifVersion != 1) ? "/motif21/libmawt" : "/motif12/libmawt");
            }
            else {
                strcpy(p, "/xawt/libmawt");
            }
            if (toolkit_name) {
                (*env)->ReleaseStringUTFChars(env, toolkit, toolkit_name);
            }
        }
        else {
            /* Default AWT Toolkit on Linux and Solaris is XAWT. */
            strcpy(p, "/xawt/libmawt");
        }

        if (toolkit) {
            (*env)->DeleteLocalRef(env, toolkit);
        }
        if (propname) {
            (*env)->DeleteLocalRef(env, propname);
        }
        if (xt_before_xm && !XAWT) {
            fprintf(stderr, "\nRuntime link error - it appears that "
                    "libXt got loaded before libXm,\n"
                    "which is not allowed.\n");
            JNU_ThrowByName(env, "java/lang/InternalError",
                            "libXt loaded before libXm");
            return JNI_VERSION_1_2;
        }
    }
 
    strcat(p, ".so");

    JNU_CallStaticMethodByName(env, NULL, "java/lang/System", "load",
                               "(Ljava/lang/String;)V",
                               JNU_NewStringPlatform(env, buf));

    awtHandle = dlopen(buf, RTLD_LAZY | RTLD_GLOBAL);

/*
  if (dlsym(awtHandle, "AWTCharRBearing") == NULL) {
  printf("========= AWTCharRBearing not found\n"); fflush(stdout);
  }
  else {
  printf("========= AWTCharRBearing was found\n"); fflush(stdout);
  }
*/

    return JNI_VERSION_1_2;
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{
    return AWT_OnLoad(vm, reserved);
}

/*
 * This entry point must remain in libawt.so as part of a contract
 * with the CDE variant of Java Media Framework. (sdtjmplay)
 * Reflect this call over to the correct libmawt.so.
 */
JNIEXPORT void JNICALL
Java_sun_awt_motif_XsessionWMcommand(JNIEnv *env, jobject this,
                                     jobject frame, jstring jcommand)
{
    /* type of the old backdoor function */
    typedef JNIEXPORT void JNICALL
        XsessionWMcommand_type(JNIEnv *env, jobject this,
                               jobject frame, jstring jcommand);

    static XsessionWMcommand_type *XsessionWMcommand = NULL;

    if (XsessionWMcommand == NULL && awtHandle == NULL) {
        return;
    }

    XsessionWMcommand = (XsessionWMcommand_type *)
        dlsym(awtHandle, "Java_sun_awt_motif_XsessionWMcommand");

    if (XsessionWMcommand == NULL)
        return;

    (*XsessionWMcommand)(env, this, frame, jcommand);
}


/*
 * This entry point must remain in libawt.so as part of a contract
 * with the CDE variant of Java Media Framework. (sdtjmplay)
 * Reflect this call over to the correct libmawt.so.
 */
JNIEXPORT void JNICALL
Java_sun_awt_motif_XsessionWMcommand_New(JNIEnv *env, jobjectArray jargv)
{
    typedef JNIEXPORT void JNICALL
        XsessionWMcommand_New_type(JNIEnv *env, jobjectArray jargv);

    static XsessionWMcommand_New_type *XsessionWMcommand = NULL;

    if (XsessionWMcommand == NULL && awtHandle == NULL) {
        return;
    }

    XsessionWMcommand = (XsessionWMcommand_New_type *)
        dlsym(awtHandle, "Java_sun_awt_motif_XsessionWMcommand_New");

    if (XsessionWMcommand == NULL)
        return;

    (*XsessionWMcommand)(env, jargv);
}


#define REFLECT_VOID_FUNCTION(name, arglist, paramlist)                 \
typedef name##_type arglist;                                            \
void name arglist                                                       \
{                                                                       \
    static name##_type *name##_ptr = NULL;                              \
    if (name##_ptr == NULL && awtHandle == NULL) {                      \
        return;                                                         \
    }                                                                   \
    name##_ptr = (name##_type *)                                        \
        dlsym(awtHandle, #name);                                        \
    if (name##_ptr == NULL) {                                           \
        return;                                                         \
    }                                                                   \
    (*name##_ptr)paramlist;                                             \
}

#define REFLECT_FUNCTION(return_type, name, arglist, paramlist)         \
typedef return_type name##_type arglist;                                \
return_type name arglist                                                \
{                                                                       \
    static name##_type *name##_ptr = NULL;                              \
    if (name##_ptr == NULL && awtHandle == NULL) {                      \
        return NULL;                                                    \
    }                                                                   \
    name##_ptr = (name##_type *)                                        \
        dlsym(awtHandle, #name);                                        \
    if (name##_ptr == NULL) {                                           \
        return NULL;                                                    \
    }                                                                   \
    return (*name##_ptr)paramlist;                                      \
}


/*
 * These entry point must remain in libawt.so ***for Java Plugin ONLY***
 * Reflect this call over to the correct libmawt.so.
 */

REFLECT_VOID_FUNCTION(getAwtLockFunctions,
                      (void (**AwtLock)(JNIEnv *), void (**AwtUnlock)(JNIEnv *),
                       void (**AwtNoFlushUnlock)(JNIEnv *), void *reserved), 
                      (AwtLock, AwtUnlock, AwtNoFlushUnlock, reserved))

REFLECT_VOID_FUNCTION(getAwtData,
                      (int32_t *awt_depth, Colormap *awt_cmap, Visual **awt_visual,
                       int32_t *awt_num_colors, void *pReserved),
                      (awt_depth, awt_cmap, awt_visual,
                       awt_num_colors, pReserved))

REFLECT_FUNCTION(Display *, getAwtDisplay, (void), ())

    
  
