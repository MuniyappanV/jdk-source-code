/*
 * @(#)awt_Dialog.cpp	1.116 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include "awt_Toolkit.h"
#include "awt_Dialog.h"
#include "awt_Window.h"

#include <windowsx.h>

#include "java_awt_Dialog.h"

/* IMPORTANT! Read the README.JNI file for notes on JNI converted AWT code.
 */

/************************************************************************/
// Struct for _SetIMMOption() method
struct SetIMMOptionStruct {
    jobject dialog;
    jstring option;
};
/************************************************************************
 * AwtDialog fields
 */

jfieldID AwtDialog::titleID;
jfieldID AwtDialog::undecoratedID;

#if defined(DEBUG)
// counts how many nested modal dialogs are open, a sanity
// check to ensure the somewhat complicated disable/enable
// code is working properly
int AwtModalityNestCounter = 0;
#endif

HHOOK AWTModalHook;
HHOOK AWTMouseHook;

int VisibleModalDialogsCount = 0;

/************************************************************************
 * AwtDialog class methods
 */

AwtDialog::AwtDialog() {
    m_modalWnd = NULL;
}

AwtDialog::~AwtDialog() {
    if (m_modalWnd != NULL) {
        WmEndModal();
    }
}

LPCTSTR AwtDialog::GetClassName() {
  return AWT_DIALOG_WINDOW_CLASS_NAME;
}

void AwtDialog::FillClassInfo(WNDCLASSEX *lpwc)
{
    AwtWindow::FillClassInfo(lpwc);
    //Fixed 6280303: REGRESSION: Java cup icon appears in title bar of dialogs
    // Dialog inherits icon from its owner dinamically
    lpwc->hIcon = NULL;
    lpwc->hIconSm = NULL;
}

/*
 * Create a new AwtDialog object and window.   
 */
AwtDialog* AwtDialog::Create(jobject peer, jobject parent) 
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject background = NULL;
    jobject target = NULL;
    AwtDialog* dialog = NULL;

    try {
        if (env->EnsureLocalCapacity(2) < 0) {
	    return NULL;
	}

	PDATA pData;
        AwtWindow* awtParent = NULL;
	HWND hwndParent = NULL;
	target = env->GetObjectField(peer, AwtObject::targetID);
	JNI_CHECK_NULL_GOTO(target, "null target", done);
	
	if (parent != NULL) {
	    JNI_CHECK_PEER_GOTO(parent, done);
            awtParent = (AwtWindow *)(JNI_GET_PDATA(parent));
            hwndParent = awtParent->GetHWnd();
	} else {
            // There is no way to prevent a parentless dialog from showing on
            //  the taskbar other than to specify an invisible parent and set
            //  WS_POPUP style for the dialog. Using toolkit window here. That
            //  will also excludes the dialog from appearing in window list while
            //  ALT+TAB'ing
            // From the other point, it may be confusing when the dialog without
            //  an owner is missing on the toolbar. So, do not set any fake
            //  parent window here.
//            hwndParent = AwtToolkit::GetInstance().GetHWnd();
        }
	dialog = new AwtDialog();
	
	{
	    int colorId = IS_WIN4X ? COLOR_3DFACE : COLOR_WINDOW;
	    DWORD style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
            if (hwndParent != NULL) {
                style |= WS_POPUP;
            }
	    style &= ~(WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
	    DWORD exStyle = IS_WIN4X ? WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME
	                             : 0;

	    if (GetRTL()) {
	        exStyle |= WS_EX_RIGHT | WS_EX_LEFTSCROLLBAR;
		if (GetRTLReadingOrder())
		    exStyle |= WS_EX_RTLREADING;
	    }


            if (env->GetBooleanField(target, AwtDialog::undecoratedID) == JNI_TRUE) {
                style = WS_POPUP | WS_CLIPCHILDREN;
                exStyle = 0;
                dialog->m_isUndecorated = TRUE;
            }

	    jint x = env->GetIntField(target, AwtComponent::xID);
	    jint y = env->GetIntField(target, AwtComponent::yID);
	    jint width = env->GetIntField(target, AwtComponent::widthID);
	    jint height = env->GetIntField(target, AwtComponent::heightID);

	    dialog->CreateHWnd(env, L"",
			       style, exStyle,
			       x, y, width, height,
			       hwndParent,
			       NULL,
			       ::GetSysColor(COLOR_WINDOWTEXT),
			       ::GetSysColor(colorId),
			       peer);

            dialog->RecalcNonClient();
            dialog->UpdateSystemMenu();

            /*
             * Initialize icon as inherited from parent if it exists
             */
            if (parent != NULL) {
                dialog->m_hIcon = awtParent->GetHIcon();
                dialog->m_hIconSm = awtParent->GetHIconSm();
                dialog->m_iconInherited = TRUE;
            }
            dialog->DoUpdateIcon();


	    background = env->GetObjectField(target,
					     AwtComponent::backgroundID);
	    if (background == NULL) {
 	        JNU_CallMethodByName(env, NULL,
 				     peer, "setDefaultColor", "()V");
	    }
	}
    } catch (...) {
        env->DeleteLocalRef(background);
	env->DeleteLocalRef(target);
	throw;
    }

done:
    env->DeleteLocalRef(background);
    env->DeleteLocalRef(target);

    return dialog;
}

MsgRouting AwtDialog::WmNcMouseDown(WPARAM hitTest, int x, int y, int button) {
    // By the request from Swing team, click on the Dialog's title should generate Ungrab
    if (m_grabbedWindow != NULL/* && !m_grabbedWindow->IsOneOfOwnersOf(this)*/) {
        m_grabbedWindow->Ungrab();
    }
    
    if (!IsFocusableWindow() && (button & LEFT_BUTTON) == LEFT_BUTTON) {
        // Dialog is non-maximizable
        if ((button & DBL_CLICK) == DBL_CLICK && hitTest == HTCAPTION) {
            return mrConsume;
        }
    }
    return AwtFrame::WmNcMouseDown(hitTest, x, y, button);
}

LRESULT CALLBACK AwtDialog::ModalFilterProc(int code,
                                            WPARAM wParam, LPARAM lParam)
{
    HWND hWnd = (HWND)wParam;
    HWND blocker = AwtWindow::GetModalBlocker(hWnd);
    if (::IsWindow(blocker) &&
        ((code == HCBT_ACTIVATE) ||
         (code == HCBT_SETFOCUS)))
    {
        // fix for 6270632: this window and all its blockers can be minimized by
        // "show desktop" button, so we should restore them first
        if (::IsIconic(hWnd)) {
            ::ShowWindow(hWnd, SW_RESTORE);
        }
        PopupAllDialogs(blocker, TRUE, ::GetForegroundWindow(), FALSE);        
	// return 1 to prevent the system from allowing the operation
        return 1;
    }
    return CallNextHookEx(0, code, wParam, lParam);
}

LRESULT CALLBACK AwtDialog::MouseHookProc(int nCode,
                                          WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        MOUSEHOOKSTRUCT *mhs = (MOUSEHOOKSTRUCT *)lParam;
        HWND hWnd = mhs->hwnd;
        if ((wParam == WM_LBUTTONDOWN) ||
            (wParam == WM_MBUTTONDOWN) ||
            (wParam == WM_RBUTTONDOWN) ||
            (wParam == WM_MOUSEACTIVATE) ||
            (wParam == WM_MOUSEWHEEL) ||
            (wParam == WM_NCLBUTTONDOWN) ||
            (wParam == WM_NCMBUTTONDOWN) ||
            (wParam == WM_NCRBUTTONDOWN))
        {
            HWND blocker = AwtWindow::GetModalBlocker(AwtComponent::GetTopLevelParentForWindow(hWnd));
            if (::IsWindow(blocker)) {
                BOOL onTaskbar = !(::WindowFromPoint(mhs->pt) == hWnd);
                PopupAllDialogs(hWnd, FALSE, ::GetForegroundWindow(), onTaskbar);
                // return a nonzero value to prevent the system from passing
                // the message to the target window procedure
                return 1;
            }
        }
    }

    return CallNextHookEx(0, nCode, wParam, lParam);
}

/*
 * The function goes through the heirarchy of the blocker dialogs and
 * popups all the dialogs. Note that the function starts from the top
 * blocker dialog and goes down to the dialog which is the bottom dialog.
 * Using another traversal may cause to the flickering issue as a bottom
 * dialog will cover a top dialog for some period of time.
 */
void AwtDialog::PopupAllDialogs(HWND dialog, BOOL isModalHook, HWND prevFGWindow, BOOL onTaskbar)
{
    HWND blocker = AwtWindow::GetModalBlocker(dialog);
    BOOL isBlocked = ::IsWindow(blocker);
    if (isBlocked) {
        PopupAllDialogs(blocker, isModalHook, prevFGWindow, onTaskbar);
    }
    PopupOneDialog(dialog, blocker, isModalHook, prevFGWindow, onTaskbar);
}

/*
 * The function popups the dialog, it distinguishes non-blocked dialogs
 * and activates the dialogs (sets as foreground window). If the dialog is
 * blocked, then it changes the Z-order of the dialog. 
 */
void AwtDialog::PopupOneDialog(HWND dialog, HWND blocker, BOOL isModalHook, HWND prevFGWindow, BOOL onTaskbar)
{
    if (dialog == AwtToolkit::GetInstance().GetHWnd()) {
        return;     
    }

    // fix for 6494032
    if (isModalHook && !::IsWindowVisible(dialog)) {
        ::ShowWindow(dialog, SW_SHOWNA);
    }

    BOOL isBlocked = ::IsWindow(blocker);
    UINT flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE;

    if (isBlocked) {
        ::SetWindowPos(dialog, blocker, 0, 0, 0, 0, flags);
    } else {
        ::SetWindowPos(dialog, HWND_TOP, 0, 0, 0, 0, flags);
        // no beep/flash if the mouse was clicked in the taskbar menu
        // or the dialog is currently inactive
        if (!isModalHook && !onTaskbar && (dialog == prevFGWindow)) {
            AnimateModalBlocker(dialog);
        }
        ::BringWindowToTop(dialog);
        ::SetForegroundWindow(dialog);
    }
}

void AwtDialog::AnimateModalBlocker(HWND window)
{
    ::MessageBeep(MB_OK);
    // some heuristics: 3 times x 64 milliseconds
    AwtWindow::FlashWindowEx(window, 3, 64, FLASHW_CAPTION);
}

LRESULT CALLBACK AwtDialog::MouseHookProc_NonTT(int nCode,
                                                WPARAM wParam, LPARAM lParam)
{
    static HWND lastHWnd = NULL;
    if (nCode >= 0)
    {
        MOUSEHOOKSTRUCT *mhs = (MOUSEHOOKSTRUCT *)lParam;
        HWND hWnd = mhs->hwnd;
        HWND blocker = AwtWindow::GetModalBlocker(AwtComponent::GetTopLevelParentForWindow(hWnd));
        if (::IsWindow(blocker)) {
            if ((wParam == WM_MOUSEMOVE) ||
                (wParam == WM_NCMOUSEMOVE))
            {
                if (lastHWnd != hWnd) {
                    static HCURSOR hArrowCur = ::LoadCursor(NULL, IDC_ARROW);
                    ::SetCursor(hArrowCur);
                    lastHWnd = hWnd;
                }
                ::PostMessage(hWnd, WM_SETCURSOR, (WPARAM)hWnd, 0);
            } else if (wParam == WM_MOUSELEAVE) {
                lastHWnd = NULL;
            }

            AwtDialog::MouseHookProc(nCode, wParam, lParam);
            return 1;
        }
    }

    return CallNextHookEx(0, nCode, wParam, lParam);
}

void AwtDialog::Show()
{
    m_visible = true;
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);  

    BOOL locationByPlatform = env->GetBooleanField(GetTarget(env), AwtWindow::locationByPlatformID);
    if (locationByPlatform) {
         moveToDefaultLocation();
    }

    if (!IsFocusableWindow()) {
        ::ShowWindow(GetHWnd(), SW_SHOWNA);
    } else {
        ::ShowWindow(GetHWnd(), SW_SHOW);
    }
}

void AwtDialog::DoUpdateIcon() 
{
    AwtFrame::DoUpdateIcon();
    //Workaround windows bug:
    //Decorations are not updated correctly for owned dialogs 
    //when changing dlg with icon <--> dlg without icon 
    RECT winRect;
    RECT clientRect;
    ::GetWindowRect(GetHWnd(), &winRect);
    ::GetClientRect(GetHWnd(), &clientRect);
    ::MapWindowPoints(HWND_DESKTOP, GetHWnd(), (LPPOINT)&winRect, 2);
    HRGN winRgn = CreateRectRgnIndirect(&winRect);
    HRGN clientRgn = CreateRectRgnIndirect(&clientRect);
    ::CombineRgn(winRgn, winRgn, clientRgn, RGN_DIFF);
    ::RedrawWindow(GetHWnd(), NULL, winRgn, RDW_FRAME | RDW_INVALIDATE); 
    ::DeleteObject(winRgn);
    ::DeleteObject(clientRgn);
}

HICON AwtDialog::GetEffectiveIcon(int iconType) 
{
    HWND hOwner = ::GetWindow(GetHWnd(), GW_OWNER);
    BOOL isResizable = ((GetStyle() & WS_THICKFRAME) != 0);
    BOOL smallIcon = ((iconType == ICON_SMALL) || (iconType == 2/*ICON_SMALL2*/));
    HICON hIcon = (smallIcon) ? GetHIconSm() : GetHIcon();
    if ((hIcon == NULL) && (isResizable || (hOwner == NULL))) { 
        //Java cup icon is not loaded in window class for dialogs
        //It needs to be set explicitly for resizable dialogs
        //and ownerless dialogs
        hIcon = (smallIcon) ? AwtToolkit::GetInstance().GetAwtIconSm() : 
            AwtToolkit::GetInstance().GetAwtIcon();
    } else if ((hIcon != NULL) && IsIconInherited() && !isResizable) {
        //Non-resizable dialogs without explicitely set icon
        //Should have no icon
        hIcon = NULL;
    }
    return hIcon;
}

void AwtDialog::CheckInstallModalHook() {
    VisibleModalDialogsCount++;
    if (VisibleModalDialogsCount == 1) {
        AWTModalHook = ::SetWindowsHookEx(WH_CBT, (HOOKPROC)ModalFilterProc,
                                         0, AwtToolkit::MainThread());
        AWTMouseHook = ::SetWindowsHookEx(WH_MOUSE, (HOOKPROC)MouseHookProc,
                                         0, AwtToolkit::MainThread());
    }
}

void AwtDialog::CheckUninstallModalHook() {
    if (VisibleModalDialogsCount == 1) {
        UnhookWindowsHookEx(AWTModalHook);
        UnhookWindowsHookEx(AWTMouseHook);
    }
    VisibleModalDialogsCount--;
}

void AwtDialog::ModalPerformActivation(HWND hWnd)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);  

    AwtWindow *w = (AwtWindow *)AwtComponent::GetComponent(hWnd);
    if ((w != NULL) && w->IsEmbeddedFrame()) {
        jobject target = w->GetTarget(env);
        env->CallVoidMethod(target, AwtFrame::activateEmbeddingTopLevelMID);
        env->DeleteLocalRef(target);
    } else {
        ::BringWindowToTop(hWnd);
        ::SetForegroundWindow(hWnd);
    }
}

void AwtDialog::ModalActivateNextWindow(HWND dialogHWnd,
                                        jobject dialogTarget, jobject dialogPeer)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2); 

    jclass wwindowPeerCls = env->FindClass("sun/awt/windows/WWindowPeer");
    jmethodID getActiveWindowsMID = env->GetStaticMethodID(wwindowPeerCls,
                                                           "getActiveWindowHandles", "()[J");
    DASSERT(getActiveWindowsMID != NULL);
    jlongArray windows = (jlongArray)(env->CallStaticObjectMethod(wwindowPeerCls,
                                                                  getActiveWindowsMID));

    if (windows == NULL) {
        return;
    }

    jboolean isCopy;
    jlong *ws = env->GetLongArrayElements(windows, &isCopy);
    int windowsCount = env->GetArrayLength(windows);
    for (int i = windowsCount - 1; i >= 0; i--) {
        HWND w = (HWND)ws[i];
        if ((w != dialogHWnd) && ModalCanBeActivated(w)) {
            AwtDialog::ModalPerformActivation(w);
            break;
        }
    }
    env->ReleaseLongArrayElements(windows, ws, 0);

    env->DeleteLocalRef(windows);
    env->DeleteLocalRef(wwindowPeerCls);
}

MsgRouting AwtDialog::WmShowModal()
{
    DASSERT(::GetCurrentThreadId() == AwtToolkit::MainThread());

    // fix for 6213128: release capture (got by popups, choices, etc) when
    // modal dialog is shown
    HWND capturer = ::GetCapture();
    if (capturer != NULL) {
      ::ReleaseCapture();
    }

    SendMessage(WM_AWT_COMPONENT_SHOW);

    CheckInstallModalHook();

    m_modalWnd = GetHWnd();

    return mrConsume;
}

MsgRouting AwtDialog::WmEndModal()
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);  

    DASSERT( ::GetCurrentThreadId() == AwtToolkit::MainThread() );
    DASSERT( ::IsWindow(m_modalWnd) );

    m_modalWnd = NULL;

    CheckUninstallModalHook();

    HWND parentHWnd = ::GetParent(GetHWnd());
    jobject peer = GetPeer(env);
    jobject target = GetTarget(env);
    if (::GetForegroundWindow() == GetHWnd()) {
        ModalActivateNextWindow(GetHWnd(), target, peer);
    }
    // hide the dialog
    SendMessage(WM_AWT_COMPONENT_HIDE);

    env->DeleteLocalRef(target);

    return mrConsume;
}

void AwtDialog::SetResizable(BOOL isResizable)
{
    // call superclass
    AwtFrame::SetResizable(isResizable);

    LONG    style = GetStyle();
    LONG    xstyle = GetStyleEx();
    if (isResizable || IsUndecorated()) {
    // remove modal frame
        xstyle &= ~WS_EX_DLGMODALFRAME;
    } else {
    // add modal frame
        xstyle |= WS_EX_DLGMODALFRAME;
    }
    // dialogs are never minimizable/maximizable, so remove those bits
    style &= ~(WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
    SetStyle(style);
    SetStyleEx(xstyle);
    RedrawNonClient();
}

// Adjust system menu so that:
//  Non-resizable dialogs only have Move and Close items
//  Resizable dialogs also have Size
// Normally, Win32 dialog system menu handling is done via
// CreateDialog/DefDlgProc, but our dialogs are using DefWindowProc
// so we handle the system menu ourselves
// fixes 4057529 - robi.khan@eng
void AwtDialog::UpdateSystemMenu()
{
    // parentless dialogs behave like frames
    if (GetOwningFrameOrDialog() == NULL) {
        return;
    }

    HWND    hWndSelf = GetHWnd();
    BOOL    isResizable = IsResizable();
    BOOL    isMaximized = ::IsZoomed(hWndSelf);

    // before restoring the default menu, check if there is an
    // InputMethodManager menu item already.  Note that it assumes
    // that the length of the InputMethodManager menu item string
    // should not be longer than 256 bytes.
    MENUITEMINFO  mii;
    memset(&mii, 0, sizeof(MENUITEMINFO));
    TCHAR         immItem[256];
    BOOL          hasImm;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_TYPE;
    mii.cch = sizeof(immItem);
    mii.dwTypeData = immItem;
    hasImm = ::GetMenuItemInfo(GetSystemMenu(hWndSelf, FALSE),
                               SYSCOMMAND_IMM, FALSE, &mii);

    // restore the default menu
    ::GetSystemMenu(hWndSelf, TRUE);
    // now get a working copy of the menu
    HMENU   hMenuSys = GetSystemMenu(hWndSelf, FALSE);
    // remove inapplicable sizing commands
    ::DeleteMenu(hMenuSys, SC_MINIMIZE, MF_BYCOMMAND);
    ::DeleteMenu(hMenuSys, SC_RESTORE, MF_BYCOMMAND);
    ::DeleteMenu(hMenuSys, SC_MAXIMIZE, MF_BYCOMMAND);
    if (!isResizable) {
	::DeleteMenu(hMenuSys, SC_SIZE, MF_BYCOMMAND);
    }
    // remove separator if only 3 items left (Move, Separator, and Close)
    if (::GetMenuItemCount(hMenuSys) == 3) {
	MENUITEMINFO	mi;
        memset(&mi, 0, sizeof(MENUITEMINFO));
	mi.cbSize = sizeof(MENUITEMINFO);
	mi.fMask = MIIM_TYPE;
	::GetMenuItemInfo(hMenuSys, 1, TRUE, &mi);
	if (mi.fType & MFT_SEPARATOR) {
	    ::DeleteMenu(hMenuSys, 1, MF_BYPOSITION);
	}
    }

    // if there was the InputMethodManager menu item, restore it.
    if (hasImm)
        ::AppendMenu(hMenuSys, MF_STRING, SYSCOMMAND_IMM, immItem);
}

// Override WmStyleChanged to adjust system menu for sizable/non-resizable dialogs
MsgRouting AwtDialog::WmStyleChanged(int wStyleType, LPSTYLESTRUCT lpss)
{
    UpdateSystemMenu();
    DoUpdateIcon();
    return mrConsume;
}

MsgRouting AwtDialog::WmSize(UINT type, int w, int h)
{
    UpdateSystemMenu(); // adjust to reflect restored vs. maximized state
    return AwtFrame::WmSize(type, w, h);
}

LRESULT AwtDialog::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    MsgRouting mr = mrDoDefault;
    LRESULT retValue = 0L;

    switch(message) {
        case WM_AWT_DLG_SHOWMODAL:
            mr = WmShowModal();
            break;
        case WM_AWT_DLG_ENDMODAL:
            mr = WmEndModal();
            break;
    }

    if (mr != mrConsume) {
        retValue = AwtFrame::WindowProc(message, wParam, lParam);
    }
    return retValue;
}

void AwtDialog::_ShowModal(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject self = (jobject)param;

    AwtDialog *d = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    d = (AwtDialog *)pData;
    if (::IsWindow(d->GetHWnd())) {
        d->SendMessage(WM_AWT_DLG_SHOWMODAL);
    }
ret:
    env->DeleteGlobalRef(self);
}

void AwtDialog::_EndModal(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject self = (jobject)param;

    AwtDialog *d = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    d = (AwtDialog *)pData;
    if (::IsWindow(d->GetHWnd())) {
        d->SendMessage(WM_AWT_DLG_ENDMODAL);
    }
ret:
    env->DeleteGlobalRef(self);
}

void AwtDialog::_SetIMMOption(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SetIMMOptionStruct *sios = (SetIMMOptionStruct *)param;
    jobject self = sios->dialog;
    jstring option = sios->option;

    int badAlloc = 0;
    LPCTSTR coption;
    LPTSTR empty = TEXT("InputMethod");
    AwtDialog *d = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    JNI_CHECK_NULL_GOTO(option, "null IMMOption", ret);

    d = (AwtDialog *)pData;
    if (::IsWindow(d->GetHWnd()))
    {
        coption = JNU_GetStringPlatformChars(env, option, NULL);
        if (coption == NULL)
        {
            badAlloc = 1;
        }
        if (!badAlloc)
        {
            HMENU hSysMenu = ::GetSystemMenu(d->GetHWnd(), FALSE);
            ::AppendMenu(hSysMenu,  MF_STRING, SYSCOMMAND_IMM, coption);
        
            if (coption != empty)
            {
                JNU_ReleaseStringPlatformChars(env, option, coption);
            }
        }
    }
ret:
    env->DeleteGlobalRef(self);
    env->DeleteGlobalRef(option);

    delete sios;

    if (badAlloc)
    {
        throw std::bad_alloc();
    }
}

/************************************************************************
 * Dialog native methods
 */

extern "C" {

JNIEXPORT void JNICALL
Java_java_awt_Dialog_initIDs(JNIEnv *env, jclass cls)
{
    TRY;

    /* java.awt.Dialog fields and methods */
    AwtDialog::titleID
        = env->GetFieldID(cls, "title", "Ljava/lang/String;");
    AwtDialog::undecoratedID
        = env->GetFieldID(cls,"undecorated","Z");

    DASSERT(AwtDialog::undecoratedID != NULL);     
    DASSERT(AwtDialog::titleID != NULL); 

    CATCH_BAD_ALLOC;
}

} /* extern "C" */


/************************************************************************
 * DialogPeer native methods
 */

extern "C" {

/*
 * Class:     sun_awt_windows_WDialogPeer
 * Method:    create
 * Signature: (Lsun/awt/windows/WComponentPeer;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WDialogPeer_create(JNIEnv *env, jobject self,
					jobject parent) 
{
    TRY;

    PDATA pData;
    AwtToolkit::CreateComponent(self, parent, 
				(AwtToolkit::ComponentFactory)
				AwtDialog::Create);
    JNI_CHECK_PEER_CREATION_RETURN(self);

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WDialogPeer
 * Method:    _show
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WDialogPeer_showModal(JNIEnv *env, jobject self)
{
    TRY;

    jobject selfGlobalRef = env->NewGlobalRef(self);

    AwtToolkit::GetInstance().SyncCall(AwtDialog::_ShowModal,
        (void *)selfGlobalRef);
    // selfGlobalRef is deleted in _ShowModal

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WDialogPeer
 * Method:    _hide
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_sun_awt_windows_WDialogPeer_endModal(JNIEnv *env, jobject self)
{
    TRY;

    jobject selfGlobalRef = env->NewGlobalRef(self);

    AwtToolkit::GetInstance().SyncCall(AwtDialog::_EndModal,
        (void *)selfGlobalRef);
    // selfGlobalRef is deleted in _EndModal

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    pSetIMMOption
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WDialogPeer_pSetIMMOption(JNIEnv *env, jobject self,
					       jstring option) 
{
    TRY;

    SetIMMOptionStruct *sios = new SetIMMOptionStruct;
    sios->dialog = env->NewGlobalRef(self);
    sios->option = (jstring)env->NewGlobalRef(option);

    AwtToolkit::GetInstance().SyncCall(AwtDialog::_SetIMMOption, sios);
    // global refs and sios are deleted in _SetIMMOption

    CATCH_BAD_ALLOC;
}
} /* extern "C" */
