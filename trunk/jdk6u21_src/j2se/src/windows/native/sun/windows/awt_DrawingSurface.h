/*
 * @(#)awt_DrawingSurface.h	1.24 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#ifndef AWT_DRAWING_SURFACE_H
#define AWT_DRAWING_SURFACE_H

#include "stdhdrs.h"
#include <jawt.h>
#include <jawt_md.h>
#include "awt_Component.h"
#include <ddraw.h>

class JAWTDrawingSurface;
class JAWTOffscreenDrawingSurface;
class JAWTOffscreenDrawingSurfaceInfo;

/*
 * New structure for 1.4.1_02 release that allows access to 
 * offscreen drawing surfaces.
 * This structure is slightly different from the old Win32
 * structure because the type of information we pass back
 * to the caller is dependent upon runtime configuration.
 * For example, we may have a DirectX7 surface pointer if
 * the runtime system had DX7 installed, but we may only have
 * a DirectX5 surface if that was the latest version installed.
 * We may also, in the future, offer different types of
 * surface, such as OpenGL surfaces, based on runtime
 * configuration and user options.
 * Given this variability, the correct usage of this structure
 * involves checking the surfaceType variable to see what type
 * of pointer we have returned and then casting the lpSurface
 * variable and using it based on the surfaceType.
 */
typedef struct jawt_Win32OffscreenDrawingSurfaceInfo {
    IDirectDrawSurface	*dxSurface;
    IDirectDrawSurface7	*dx7Surface;
} JAWT_Win32OffscreenDrawingSurfaceInfo;


/*
 * Drawing surface info houses the important drawing information.
 * Here we multiply inherit from both structures, the platform-specific
 * and the platform-independent versions, so they are treated as the
 * same object.
 */
class JAWTDrawingSurfaceInfo : public JAWT_Win32DrawingSurfaceInfo,
    public JAWT_DrawingSurfaceInfo {
public:
    jint Init(JAWTDrawingSurface* parent);
public:
    JAWT_Rectangle clipRect;
};

/*
 * Same as above except for offscreen surfaces instead of onscreen
 * Components.
 */
class JAWTOffscreenDrawingSurfaceInfo : 
    public JAWT_Win32OffscreenDrawingSurfaceInfo,
    public JAWT_DrawingSurfaceInfo 
{
public:
    jint Init(JAWTOffscreenDrawingSurface* parent);

};

/*
 * The drawing surface wrapper.
 */
class JAWTDrawingSurface : public JAWT_DrawingSurface {
public:
    JAWTDrawingSurface() {}
    JAWTDrawingSurface(JNIEnv* env, jobject rTarget);
    virtual ~JAWTDrawingSurface();

public:
    JAWTDrawingSurfaceInfo info;

// Static function pointers
public:
    static jint JNICALL LockSurface
        (JAWT_DrawingSurface* ds);

    static JAWT_DrawingSurfaceInfo* JNICALL GetDSI
        (JAWT_DrawingSurface* ds);

    static void JNICALL FreeDSI
        (JAWT_DrawingSurfaceInfo* dsi);

    static void JNICALL UnlockSurface
        (JAWT_DrawingSurface* ds);
};

/*
 * Same as above except for offscreen surfaces instead of onscreen
 * Components.
 */
class JAWTOffscreenDrawingSurface : public JAWTDrawingSurface {
public:
    JAWTOffscreenDrawingSurface() {}
    JAWTOffscreenDrawingSurface(JNIEnv* env, jobject rTarget);
    virtual ~JAWTOffscreenDrawingSurface();

public:
    JAWTOffscreenDrawingSurfaceInfo info;

// Static function pointers
public:
    static JAWT_DrawingSurfaceInfo* JNICALL GetDSI
        (JAWT_DrawingSurface* ds);

    static void JNICALL FreeDSI
        (JAWT_DrawingSurfaceInfo* dsi);

    static jint JNICALL LockSurface
        (JAWT_DrawingSurface* ds);

    static void JNICALL UnlockSurface
        (JAWT_DrawingSurface* ds);
};

#ifdef __cplusplus
extern "C" {
#endif
    _JNI_IMPORT_OR_EXPORT_
    JAWT_DrawingSurface* JNICALL DSGetDrawingSurface
        (JNIEnv* env, jobject target);

    _JNI_IMPORT_OR_EXPORT_
    void JNICALL DSFreeDrawingSurface
        (JAWT_DrawingSurface* ds);

    _JNI_IMPORT_OR_EXPORT_
    void JNICALL DSLockAWT(JNIEnv* env);

    _JNI_IMPORT_OR_EXPORT_
    void JNICALL DSUnlockAWT(JNIEnv* env);

    _JNI_IMPORT_OR_EXPORT_
    jobject JNICALL DSGetComponent(JNIEnv* env, void* platformInfo);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AWT_DRAWING_SURFACE_H */
