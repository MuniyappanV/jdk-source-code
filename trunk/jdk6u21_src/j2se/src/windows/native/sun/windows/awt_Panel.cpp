/*
 * @(#)awt_Panel.cpp	1.16 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include "awt_Panel.h"
#include "awt_Toolkit.h"
#include "awt_Component.h"
#include "awt.h"

/************************************************************************
 * AwtPanel fields
 */

jfieldID AwtPanel::insets_ID;

static char* AWTPANEL_RESTACK_MSG_1 = "Peers array is null";
static char* AWTPANEL_RESTACK_MSG_2 = "Peer null in JNI";
static char* AWTPANEL_RESTACK_MSG_3 = "Native resources unavailable";
static char* AWTPANEL_RESTACK_MSG_4 = "Child peer is null";

void* AwtPanel::Restack(void * param) {
    TRY;

    JNIEnv* env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    jobjectArray peers = (jobjectArray)param;

    int peerCount = env->GetArrayLength(peers);
    if (peerCount < 1) {
        env->DeleteGlobalRef(peers);
        return AWTPANEL_RESTACK_MSG_1; 
    }

    jobject self = env->GetObjectArrayElement(peers, 0);
    // It's entirely possible that our native resources have been destroyed
    // before our java peer - if we're dispose()d, for instance.
    // Alert caller w/ IllegalComponentStateException.
    if (self == NULL) {
        env->DeleteGlobalRef(peers);
        return AWTPANEL_RESTACK_MSG_2;
    }
    PDATA pData = JNI_GET_PDATA(self);
    if (pData == NULL) {
        env->DeleteGlobalRef(peers);
        env->DeleteLocalRef(self);
        return AWTPANEL_RESTACK_MSG_3;
    }

    AwtPanel* panel = (AwtPanel*)pData;

    HWND prevWindow = 0;

    for (int i = 1; i < peerCount; i++) {
        jobject peer = env->GetObjectArrayElement(peers, i);
        if (peer == NULL) {
            // Nonsense
            env->DeleteGlobalRef(peers);
            env->DeleteLocalRef(self);
            return  AWTPANEL_RESTACK_MSG_4;
        }
        PDATA child_pData = JNI_GET_PDATA(peer);
        if (child_pData == NULL) {
            env->DeleteLocalRef(peer);
            env->DeleteGlobalRef(peers);
            env->DeleteLocalRef(self);
            return AWTPANEL_RESTACK_MSG_3;
        }
        AwtComponent* child_comp = (AwtComponent*)child_pData;
        ::SetWindowPos(child_comp->GetHWnd(), prevWindow, 0, 0, 0, 0, 
                       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_DEFERERASE | SWP_ASYNCWINDOWPOS);
        prevWindow = child_comp->GetHWnd();
        env->DeleteLocalRef(peer);
    }
    env->DeleteGlobalRef(peers);
    env->DeleteLocalRef(self);

    CATCH_BAD_ALLOC_RET("Allocation error");
    return NULL;
}

/************************************************************************
 * AwtPanel native methods
 */

extern "C" {

JNIEXPORT void JNICALL 
Java_sun_awt_windows_WPanelPeer_initIDs(JNIEnv *env, jclass cls) {

    TRY;

    AwtPanel::insets_ID = env->GetFieldID(cls, "insets_", "Ljava/awt/Insets;");

    DASSERT(AwtPanel::insets_ID != NULL);

    CATCH_BAD_ALLOC;
}

JNIEXPORT void JNICALL 
Java_sun_awt_windows_WPanelPeer_pRestack(JNIEnv *env, jobject self, jobjectArray peers) {

    TRY;

    const char * error = (const char*)AwtToolkit::GetInstance().InvokeFunction(AwtPanel::Restack, env->NewGlobalRef(peers));
    if (error != NULL) {
        JNU_ThrowByName(env, "java/awt/IllegalComponentStateException", error);
    }

    CATCH_BAD_ALLOC;
}


} /* extern "C" */

