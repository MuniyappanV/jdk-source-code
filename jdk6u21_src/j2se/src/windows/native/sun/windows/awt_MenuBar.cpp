/*
 * @(#)awt_MenuBar.cpp	1.44 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include "awt_MenuBar.h"
#include "awt_Frame.h"

/* IMPORTANT! Read the README.JNI file for notes on JNI converted AWT code.
 */

/***********************************************************************/
// struct for _AddMenu() method
struct AddMenuStruct {
    jobject menubar;
    jobject menu;
};
/************************************************************************
 * AwtMenuBar fields
 */

jmethodID AwtMenuBar::getMenuMID;
jmethodID AwtMenuBar::getMenuCountMID;


/************************************************************************
 * AwtMenuBar methods
 */


AwtMenuBar::AwtMenuBar() {
    m_frame = NULL;
}

AwtMenuBar::~AwtMenuBar() {
    m_frame = NULL;
}

LPCTSTR AwtMenuBar::GetClassName() {
  return TEXT("SunAwtMenuBar");
}

/* Create a new AwtMenuBar object and menu.   */
AwtMenuBar* AwtMenuBar::Create(jobject self, jobject framePeer)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject target = NULL;
    AwtMenuBar* menuBar = NULL;

    try {
        if (env->EnsureLocalCapacity(1) < 0) {
	    return NULL;
	}

	/* target is a java.awt.MenuBar */
	target = env->GetObjectField(self, AwtObject::targetID);
	JNI_CHECK_NULL_GOTO(target, "null target", done);

	menuBar = new AwtMenuBar();

        SetLastError(0);
        HMENU hMenu = ::CreateMenu();
        // fix for 5088782
        if (!CheckMenuCreation(env, self, hMenu))
        {
            env->DeleteLocalRef(target);
            return NULL;
        }

        menuBar->SetHMenu(hMenu);

	menuBar->LinkObjects(env, self);
        if (framePeer != NULL) {
            PDATA pData;
            JNI_CHECK_PEER_GOTO(framePeer, done);
            menuBar->m_frame = (AwtFrame *)pData;
        } else {
            menuBar->m_frame = NULL;
        }
    } catch (...) {
        env->DeleteLocalRef(target);
	throw;
    }

done:
    if (target != NULL) {
        env->DeleteLocalRef(target);
    }

    return menuBar;
}

HWND AwtMenuBar::GetOwnerHWnd()
{
    AwtFrame *myFrame = m_frame;
    if (myFrame == NULL)
        return NULL;
    else
        return myFrame->GetHWnd();
}

void AwtMenuBar::SendDrawItem(AwtMenuItem* awtMenuItem,
			      DRAWITEMSTRUCT& drawInfo)
{
    awtMenuItem->DrawItem(drawInfo);
}

void AwtMenuBar::SendMeasureItem(AwtMenuItem* awtMenuItem,
				 HDC hDC, MEASUREITEMSTRUCT& measureInfo)
{
    awtMenuItem->MeasureItem(hDC, measureInfo);
}

int AwtMenuBar::CountItem(jobject menuBar)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    jint nCount = env->CallIntMethod(menuBar, AwtMenuBar::getMenuCountMID);
    DASSERT(!safe_ExceptionOccurred(env));

    return nCount;
}

AwtMenuItem* AwtMenuBar::GetItem(jobject target, long index)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    if (env->EnsureLocalCapacity(2) < 0) {
	return NULL;
    }

    jobject menu = env->CallObjectMethod(target, AwtMenuBar::getMenuMID,index);
    DASSERT(!safe_ExceptionOccurred(env));

    jobject menuItemPeer = GetPeerForTarget(env, menu);
    PDATA pData;
    JNI_CHECK_PEER_RETURN_NULL(menuItemPeer);
    AwtMenuItem* awtMenuItem = (AwtMenuItem*)pData;

    env->DeleteLocalRef(menu);
    env->DeleteLocalRef(menuItemPeer);

    return awtMenuItem;
}

void AwtMenuBar::DrawItem(DRAWITEMSTRUCT& drawInfo)
{
    DASSERT(drawInfo.CtlType == ODT_MENU);
    AwtMenu::DrawItems(drawInfo);
}

void AwtMenuBar::MeasureItem(HDC hDC,
			     MEASUREITEMSTRUCT& measureInfo)
{
    DASSERT(measureInfo.CtlType == ODT_MENU);
    AwtMenu::MeasureItem(hDC, measureInfo);
}

void AwtMenuBar::AddItem(AwtMenuItem* item)
{
    AwtMenu::AddItem(item);
    HWND hOwnerWnd = GetOwnerHWnd();
    if (hOwnerWnd != NULL) {
        VERIFY(::InvalidateRect(hOwnerWnd,0,TRUE));
    }
}

void AwtMenuBar::DeleteItem(UINT index)
{
    AwtMenu::DeleteItem(index);
    HWND hOwnerWnd = GetOwnerHWnd();
    if (hOwnerWnd != NULL) {
        VERIFY(::InvalidateRect(hOwnerWnd,0,TRUE));
    }
    ::DrawMenuBar(GetOwnerHWnd());
}

LRESULT AwtMenuBar::WinThreadExecProc(ExecuteArgs * args)
{
    switch( args->cmdId ) {
	case MENUBAR_DELITEM:
	    this->DeleteItem(static_cast<UINT>(args->param1));
	    break;

	default:
	    AwtMenu::WinThreadExecProc(args);
	    break;
    }
    return 0L;
}

void AwtMenuBar::_AddMenu(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    AddMenuStruct *ams = (AddMenuStruct *)param;
    jobject self = ams->menubar;
    jobject menu = ams->menu;

    AwtMenuBar *m = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    JNI_CHECK_NULL_GOTO(menu, "null menu", ret);
    m = (AwtMenuBar *)pData;
    if (::IsWindow(m->GetOwnerHWnd()))
    {
        /* The menu was already created and added during peer creation -- redraw */
        ::DrawMenuBar(m->GetOwnerHWnd());
    }
ret:
    env->DeleteGlobalRef(self);
    if (menu != NULL) {
        env->DeleteGlobalRef(menu);
    }

    delete ams;
}

/************************************************************************
 * MenuBar native methods
 */

extern "C" {

/*
 * Class:     java_awt_MenuBar
 * Method:    initIDs
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_java_awt_MenuBar_initIDs(JNIEnv *env, jclass cls)
{
    TRY;
  
    AwtMenuBar::getMenuCountMID = env->GetMethodID(cls, "getMenuCountImpl", "()I");
    AwtMenuBar::getMenuMID = env->GetMethodID(cls, "getMenuImpl",
					      "(I)Ljava/awt/Menu;");
    DASSERT(AwtMenuBar::getMenuCountMID != NULL);
    DASSERT(AwtMenuBar::getMenuMID != NULL);

    CATCH_BAD_ALLOC;
}

} /* extern "C" */


/************************************************************************
 * WMenuBarPeer native methods
 */

extern "C" {

/*
 * Class:     sun_awt_windows_WMenuBarPeer
 * Method:    addMenu
 * Signature: (Ljava/awt/Menu;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WMenuBarPeer_addMenu(JNIEnv *env, jobject self,
					  jobject menu)
{
    TRY;

    AddMenuStruct *ams = new AddMenuStruct;
    ams->menubar = env->NewGlobalRef(self);
    ams->menu = env->NewGlobalRef(menu);

    AwtToolkit::GetInstance().SyncCall(AwtMenuBar::_AddMenu, ams);
    // global refs and ams are deleted in _AddMenu()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WMenuBarPeer
 * Method:    delMenu
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WMenuBarPeer_delMenu(JNIEnv *env, jobject self,
					  jint index)
{
    TRY;

    PDATA pData;
    JNI_CHECK_PEER_RETURN(self);
    AwtObject::WinThreadExec(self, AwtMenuBar::MENUBAR_DELITEM, (LPARAM)index);

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WMenuBarPeer
 * Method:    create
 * Signature: (Lsun/awt/windows/WFramePeer;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WMenuBarPeer_create(JNIEnv *env, jobject self,
					 jobject frame)
{
    TRY;

    AwtToolkit::CreateComponent(self, frame,
				(AwtToolkit::ComponentFactory)
				AwtMenuBar::Create);
    PDATA pData;
    JNI_CHECK_PEER_CREATION_RETURN(self);

    CATCH_BAD_ALLOC;
}

} /* extern "C" */