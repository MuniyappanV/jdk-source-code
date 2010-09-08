/*
 * @(#)awt_MenuBar.c	1.41 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
#ifdef HEADLESS
    #error This file should not be included in headless library
#endif

#include "awt_p.h"
#include "java_awt_MenuBar.h"
#include "sun_awt_motif_MMenuBarPeer.h"
#include "java_awt_Menu.h"
#include "java_awt_Frame.h"
#include "sun_awt_motif_MFramePeer.h"

#include "awt_GraphicsEnv.h"
#include "awt_MenuBar.h"
#include "awt_Component.h"

#include <jni.h>
#include <jni_util.h>

extern struct MComponentPeerIDs mComponentPeerIDs;
extern struct X11GraphicsConfigIDs x11GraphicsConfigIDs;
struct MMenuBarPeerIDs mMenuBarPeerIDs;

/*
 * Class:     sun_awt_motif_MMenuBarPeer
 * Method:    initIDs
 * Signature: ()V
 */

/* This function gets called from the static initializer for MMenuBarPeer.java
   to initialize the fieldIDs fields that may be accessed from C */
JNIEXPORT void JNICALL 
Java_sun_awt_motif_MMenuBarPeer_initIDs
  (JNIEnv *env, jclass cls)
{
  mMenuBarPeerIDs.pData = (*env)->GetFieldID(env, cls, "pData", "J");
  mMenuBarPeerIDs.graphicsConfig =
      (*env)->GetFieldID(env, cls, "graphicsConfig", 
			 "Lsun/awt/X11GraphicsConfig;");
}

static AwtGraphicsConfigDataPtr
copyGraphicsConfigToMenuBarPeer(
JNIEnv *env, jobject frame, jobject thisMenuBar) {
    
    jobject gc_object;
    AwtGraphicsConfigDataPtr adata;
    
    /* GraphicsConfiguration object of Component */
    gc_object = (*env)->GetObjectField(env, frame, 
                                       mComponentPeerIDs.graphicsConfig);
    
    if (gc_object != NULL) {
        /* Set graphicsConfig field of MComponentPeer */
        (*env)->SetObjectField (env, thisMenuBar,
                                mMenuBarPeerIDs.graphicsConfig,
                                gc_object);
        adata = (AwtGraphicsConfigDataPtr)
            JNU_GetLongFieldAsPtr(env, gc_object, 
                                  x11GraphicsConfigIDs.aData);
    } else {
        /* Component was not constructed with a GraphicsConfiguration
           object */
        adata = getDefaultConfig(DefaultScreen(awt_display));
    }
    
    return adata;
}

AwtGraphicsConfigDataPtr
getGraphicsConfigFromMenuBarPeer(JNIEnv *env, jobject menubarPeer) {
    
    jobject gc_object;
    AwtGraphicsConfigDataPtr adata;
    
    /* GraphicsConfiguration object of Component */
    gc_object = (*env)->GetObjectField(env, menubarPeer,
                                       mMenuBarPeerIDs.graphicsConfig);
    
    if (gc_object != NULL) {
        adata = (AwtGraphicsConfigDataPtr)
            JNU_GetLongFieldAsPtr(env, gc_object, 
                                  x11GraphicsConfigIDs.aData);
    } else {
        adata = getDefaultConfig(DefaultScreen(awt_display));
    }
    
    return adata;
}

/*
 * Class:     sun_awt_motif_MMenuBarPeer
 * Method:    create
 * Signature: (Lsun/awt/motif/MFramePeer;)V
 */
JNIEXPORT void JNICALL Java_sun_awt_motif_MMenuBarPeer_create
  (JNIEnv * env, jobject this, jobject frame)
{
#define MAX_ARGC 20
    Arg args[MAX_ARGC];
    int32_t argc;
    struct ComponentData *mdata;
    struct FrameData *wdata;
    Pixel bg;
    Pixel fg;
    AwtGraphicsConfigDataPtr adata;

    if (JNU_IsNull(env, frame)) {
        JNU_ThrowNullPointerException(env, "NullPointerException");
        return;
    }
    AWT_LOCK();
    wdata = (struct FrameData *)
	JNU_GetLongFieldAsPtr(env, frame, mComponentPeerIDs.pData);
    mdata = ZALLOC(ComponentData);

    if (wdata == NULL || mdata == NULL) {
        JNU_ThrowNullPointerException(env, "NullPointerException");
        AWT_UNLOCK();
        return;
    }
    JNU_SetLongFieldFromPtr(env, this, mMenuBarPeerIDs.pData, mdata);

    adata = copyGraphicsConfigToMenuBarPeer(env, frame, this);
    
    XtVaGetValues(wdata->winData.comp.widget,
                  XmNbackground, &bg,
                  XmNforeground, &fg,
                  NULL);

    argc = 0;
    XtSetArg(args[argc], XmNbackground, bg);
    argc++;
    XtSetArg(args[argc], XmNforeground, fg);
    argc++;
    XtSetArg (args[argc], XmNscreen,
              ScreenOfDisplay(awt_display,
                              adata->awt_visInfo.screen));
    argc++;

    DASSERT(!(argc > MAX_ARGC));
    mdata->widget = XmCreateMenuBar(wdata->mainWindow, "menu_bar", args, argc);
    awt_addMenuWidget(mdata->widget);
    XtSetMappedWhenManaged(mdata->widget, False);
    XtManageChild(mdata->widget);
    AWT_UNLOCK();
}

/*
 * Class:     sun_awt_motif_MMenuBarPeer
 * Method:    dispose
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_sun_awt_motif_MMenuBarPeer_pDispose
  (JNIEnv * env, jobject this)
{
    struct ComponentData *mdata;

    AWT_LOCK();

    /*hania LOOK HERE does this make sense? look at original code */
    mdata = (struct ComponentData *)
	JNU_GetLongFieldAsPtr(env, this, mMenuBarPeerIDs.pData);
    if (mdata == NULL) {
        AWT_UNLOCK();
        return;
    }
    awt_delMenuWidget(mdata->widget);
    XtUnmanageChild(mdata->widget);
    awt_util_consumeAllXEvents(mdata->widget);
    XtDestroyWidget(mdata->widget);
    free((void *) mdata);
    (*env)->SetLongField(env, this, mMenuBarPeerIDs.pData, (jlong)0);
    AWT_UNLOCK();
}

