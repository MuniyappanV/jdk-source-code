/*
 * @(#)awt_Window.cpp	1.190 10/03/23
 *
 * Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include <windowsx.h>

#include <jlong.h>

#include "awt_Component.h"
#include "awt_Container.h"
#include "awt_Frame.h"
#include "awt_Insets.h"
#include "awt_Panel.h"
#include "awt_Toolkit.h"
#include "awt_Window.h"
#include "awt_dlls.h"
#include "awt_Win32GraphicsDevice.h"
#include "awt_BitmapUtil.h"
#include "awt_IconCursor.h"
#include "ComCtl32Util.h"
#include "awt_Multimon.h"

#include "java_awt_Insets.h"
#include <java_awt_Container.h>
#include <java_awt_event_ComponentEvent.h>
#include "sun_awt_windows_WCanvasPeer.h"

#if !defined(__int3264)
typedef __int32 LONG_PTR;
#endif // __int3264

// Used for Swing's Menu/Tooltip animation Support
const int UNSPECIFIED = 0;
const int TOOLTIP = 1;
const int MENU = 2;
const int SUBMENU = 3;
const int POPUPMENU = 4;
const int COMBOBOX_POPUP = 5;
const int TYPES_COUNT = 6;
jint windowTYPES[TYPES_COUNT];


/* IMPORTANT! Read the README.JNI file for notes on JNI converted AWT code.
 */

/***********************************************************************/
// struct for _SetAlwaysOnTop() method
struct SetAlwaysOnTopStruct {
    jobject window;
    jboolean value;
};
// struct for _SetTitle() method
struct SetTitleStruct {
    jobject window;
    jstring title;
};
// struct for _SetResizable() method
struct SetResizableStruct {
    jobject window;
    jboolean resizable;
};
// struct for _UpdateInsets() method
struct UpdateInsetsStruct {
    jobject window;
    jobject insets;
};
// struct for _ReshapeFrame() method
struct ReshapeFrameStruct {
    jobject frame;
    jint x, y;
    jint w, h; 
};

// struct for _SetIconImagesData
struct SetIconImagesDataStruct {
    jobject window;
    jintArray iconRaster;
    jint w, h;
    jintArray smallIconRaster;
    jint smw, smh;
};

// struct for _SetMinSize() method
// and other methods setting sizes
struct SizeStruct {
    jobject window;
    jint w, h; 
};
// struct for _SetFocusableWindow() method
struct SetFocusableWindowStruct {
    jobject window;
    jboolean isFocusableWindow;
};
// struct for _ModalDisable() method
struct ModalDisableStruct {
    jobject window;
    jlong blockerHWnd;
};
// struct for _SetOpacity() method
struct OpacityStruct {
    jobject window;
    jint iOpacity;
};

// struct for _SetOpaque() method
struct OpaqueStruct {
    jobject window;
    jboolean isOpaque;
};

// struct for _UpdateWindow() method
struct UpdateWindowStruct {
    jobject window;
    jintArray data;
    HBITMAP hBitmap;
    jint width, height;
};

// struct for _RepositionSecurityWarning() method
struct RepositionSecurityWarningStruct {
    jobject window;
};

/************************************************************************
 * AwtWindow fields
 */

jfieldID AwtWindow::warningStringID;
jfieldID AwtWindow::locationByPlatformID;

jfieldID AwtWindow::securityWarningWidthID;
jfieldID AwtWindow::securityWarningHeightID;

jfieldID AwtWindow::sysXID;
jfieldID AwtWindow::sysYID;
jfieldID AwtWindow::sysWID;
jfieldID AwtWindow::sysHID;

jmethodID AwtWindow::getWarningStringMID;
jmethodID AwtWindow::calculateSecurityWarningPositionMID;

int AwtWindow::ms_instanceCounter = 0;
HHOOK AwtWindow::ms_hCBTFilter;
AwtWindow * AwtWindow::m_grabbedWindow = NULL;
UINT AwtWindow::untrustedWindowsCounter = 0;

/************************************************************************
 * AwtWindow class methods
 */

AwtWindow::AwtWindow() {
    m_resizing = FALSE;
    m_sizePt.x = m_sizePt.y = 0;
    m_owningFrameDialog = NULL;
    m_isResizable = FALSE;//Default value is replaced after construction
    m_minSize.x = m_minSize.y = 0;
    m_hIcon = NULL;
    m_hIconSm = NULL;
    m_iconInherited = FALSE;
    VERIFY(::SetRectEmpty(&m_insets));
    VERIFY(::SetRectEmpty(&m_old_insets));
    VERIFY(::SetRectEmpty(&m_warningRect));

    // what's the best initial value?
    m_screenNum = -1;
    ms_instanceCounter++;
    m_grabbed = FALSE;
    m_isFocusableWindow = TRUE;
    m_isRetainingHierarchyZOrder = FALSE;

    if (AwtWindow::ms_instanceCounter == 1) {
        AwtWindow::ms_hCBTFilter =
            ::SetWindowsHookEx(WH_CBT, (HOOKPROC)AwtWindow::CBTFilter,
                               0, AwtToolkit::MainThread());
    }
    m_opaque = TRUE;
    m_opacity = 0xff;

    warningString = NULL;
    warningWindow = NULL;
    securityTooltipWindow = NULL;
    securityWarningAnimationStage = 0;
    currentWmSizeState = SIZE_RESTORED;

    hContentBitmap = NULL;

    ::InitializeCriticalSection(&contentBitmapCS);
    m_alwaysOnTop = false;
}

AwtWindow::~AwtWindow() {
    // Fix 4745575 GDI Resource Leak
    // MSDN 
    // Before a window is destroyed (that is, before it returns from processing 
    // the WM_NCDESTROY message), an application must remove all entries it has 
    // added to the property list. The application must use the RemoveProp function
    // to remove the entries. 

    if (--AwtWindow::ms_instanceCounter == 0) {
        ::UnhookWindowsHookEx(AwtWindow::ms_hCBTFilter);
    }

    ::RemoveProp(GetHWnd(), ModalBlockerProp);

    if (m_grabbedWindow == this) {
        Ungrab();
    }
    if ((m_hIcon != NULL) && !m_iconInherited) {
        ::DestroyIcon(m_hIcon);
    }
    if ((m_hIconSm != NULL) && !m_iconInherited) {
        ::DestroyIcon(m_hIconSm);
    }
    if (warningString != NULL) {
        delete [] warningString;
    }
    ::EnterCriticalSection(&contentBitmapCS);
    if (hContentBitmap != NULL) {
        ::DeleteObject(hContentBitmap);
        hContentBitmap = NULL;
    }
    ::LeaveCriticalSection(&contentBitmapCS);
    ::DeleteCriticalSection(&contentBitmapCS);
}

void
AwtWindow::Grab() {
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    if (m_grabbedWindow != NULL) {
        m_grabbedWindow->Ungrab();
    }
    m_grabbed = TRUE;
    m_grabbedWindow = this;
    if (AwtComponent::GetFocusedWindow() == NULL && IsFocusableWindow()) {
        // we shouldn't perform grab in this case (see 4841881 & 6539458) 
        Ungrab();
    } else if (GetHWnd() != AwtComponent::GetFocusedWindow()) {
        _ToFront(env->NewGlobalRef(GetPeer(env)));
        // Global ref was deleted in _ToFront
    }
}

void
AwtWindow::Ungrab(BOOL doPost) {
    if (m_grabbed && m_grabbedWindow == this) {
        if (doPost) {
            PostUngrabEvent();
        }
        m_grabbedWindow = NULL;
        m_grabbed = FALSE;
    }
}

void
AwtWindow::Ungrab() {
    Ungrab(TRUE);
}

void AwtWindow::_Grab(void * param) {
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    
    jobject self = (jobject)param;

    if (env->EnsureLocalCapacity(1) < 0)
    {
        env->DeleteGlobalRef(self);
        return;
    }

    AwtWindow *p = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    p = (AwtWindow *)pData;
    p->Grab();

  ret:
    env->DeleteGlobalRef(self);
}

void AwtWindow::_Ungrab(void * param) {
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    
    jobject self = (jobject)param;

    if (env->EnsureLocalCapacity(1) < 0)
    {
        env->DeleteGlobalRef(self);
        return;
    }

    AwtWindow *p = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    p = (AwtWindow *)pData;
    p->Ungrab(FALSE);

  ret:
    env->DeleteGlobalRef(self);
}

MsgRouting AwtWindow::WmNcMouseDown(WPARAM hitTest, int x, int y, int button) {
    if (m_grabbedWindow != NULL && !m_grabbedWindow->IsOneOfOwnersOf(this)) {
        m_grabbedWindow->Ungrab();
    }
    return AwtCanvas::WmNcMouseDown(hitTest, x, y, button);
}

MsgRouting AwtWindow::WmWindowPosChanging(LPARAM windowPos) {
    return mrDoDefault;
}

void AwtWindow::RepositionSecurityWarning(JNIEnv *env)
{
    RECT rect;
    CalculateWarningWindowBounds(env, &rect);

    ::SetWindowPos(warningWindow, IsAlwaysOnTop() ? HWND_TOPMOST : GetHWnd(),
            rect.left, rect.top,
            rect.right - rect.left, rect.bottom - rect.top,
            SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER
            );
}

MsgRouting AwtWindow::WmWindowPosChanged(LPARAM windowPos) {
    WINDOWPOS * wp = (WINDOWPOS *)windowPos;

    // Reposition the warning window
    if (IsUntrusted() && warningWindow != NULL) {
        if (wp->flags & SWP_HIDEWINDOW) {
            UpdateSecurityWarningVisibility();
        }

        RepositionSecurityWarning((JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2));

        if (wp->flags & SWP_SHOWWINDOW) {
            UpdateSecurityWarningVisibility();
        }
    }

    return mrDoDefault;
}

LPCTSTR AwtWindow::GetClassName() {
  return TEXT("SunAwtWindow");
}

void AwtWindow::FillClassInfo(WNDCLASSEX *lpwc)
{
    AwtComponent::FillClassInfo(lpwc);
    /*
     * This line causes bug #4189244 (Swing Popup menu is not being refreshed (cleared) under a Dialog)
     * so it's comment out (son@sparc.spb.su)
     *
     * lpwc->style     |= CS_SAVEBITS; // improve pull-down menu performance
     */
    lpwc->cbWndExtra = DLGWINDOWEXTRA;
}

bool AwtWindow::IsWarningWindow(HWND hWnd)
{
    const UINT len = 128;
    TCHAR windowClassName[len];

    ::RealGetWindowClass(hWnd, windowClassName, len);
    return 0 == _tcsncmp(windowClassName,
            AwtWindow::GetWarningWindowClassName(), len);
}

LRESULT CALLBACK AwtWindow::CBTFilter(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HCBT_ACTIVATE || nCode == HCBT_SETFOCUS) {
        HWND hWnd = (HWND)wParam;
        AwtComponent *comp = AwtComponent::GetComponent(hWnd);

        if (comp == NULL) {
            // Check if it's a security warning icon
            // See: 5091224, 6181725, 6732583
            if (AwtWindow::IsWarningWindow(hWnd)) {
                return 1;
            }
        } else {
            if (comp->IsTopLevel()) {
                AwtWindow* win = (AwtWindow*)comp;

                if (!win->IsFocusableWindow() ||
                        (win->IsEmbeddedFrame() &&
                         !((AwtFrame*)win)->IsEmbeddedFrameActivationRequest()))
                {
                    return 1;
                }
            }
        }
    }
    return ::CallNextHookEx(AwtWindow::ms_hCBTFilter, nCode, wParam, lParam);
}

void AwtWindow::InitSecurityWarningSize(JNIEnv *env)
{
    warningWindowWidth = ::GetSystemMetrics(SM_CXSMICON);
    warningWindowHeight = ::GetSystemMetrics(SM_CYSMICON);

    jobject target = GetTarget(env);

    env->SetIntField(target, AwtWindow::securityWarningWidthID,
            warningWindowWidth);
    env->SetIntField(target, AwtWindow::securityWarningHeightID,
            warningWindowHeight);

    env->DeleteLocalRef(target);
}

void AwtWindow::CreateHWnd(JNIEnv *env, LPCWSTR title,
        DWORD windowStyle,
        DWORD windowExStyle,
        int x, int y, int w, int h,
        HWND hWndParent, HMENU hMenu,
        COLORREF colorForeground,
        COLORREF colorBackground,
        jobject peer)
{
    // Retrieve the warning string
    // Note: we need to get it before CreateHWnd() happens because
    // the isUntrusted() method may be invoked while the HWND
    // is being created in response to some window messages.
    jobject target = env->GetObjectField(peer, AwtObject::targetID);
    jstring javaWarningString =
        (jstring)env->CallObjectMethod(target, AwtWindow::getWarningStringMID);

    if (javaWarningString != NULL) {
        size_t length = env->GetStringLength(javaWarningString) + 1;
        warningString = new WCHAR[length];
        env->GetStringRegion(javaWarningString, 0,
                static_cast<jsize>(length - 1),
                reinterpret_cast<jchar*>(warningString));
        warningString[length-1] = L'\0';

        env->DeleteLocalRef(javaWarningString);
    }
    env->DeleteLocalRef(target);

    AwtCanvas::CreateHWnd(env, title,
            windowStyle,
            windowExStyle,
            x, y, w, h,
            hWndParent, hMenu,
            colorForeground,
            colorBackground,
            peer);

    // Now we need to create the warning window.
    CreateWarningWindow(env);
}

void AwtWindow::CreateWarningWindow(JNIEnv *env)
{
    if (!IsUntrusted()) {
        return;
    }

    if (++AwtWindow::untrustedWindowsCounter == 1) {
        AwtToolkit::GetInstance().InstallMouseLowLevelHook();
    }

    InitSecurityWarningSize(env);

    RECT rect;
    CalculateWarningWindowBounds(env, &rect);

    RegisterWarningWindowClass();
    warningWindow = ::CreateWindowEx(
            AWT_WS_EX_NOACTIVATE,
            GetWarningWindowClassName(),
            warningString,
            WS_POPUP,
            rect.left, rect.top,
            rect.right - rect.left, rect.bottom - rect.top,
            GetHWnd(), // owner
            NULL, // menu
            AwtToolkit::GetInstance().GetModuleHandle(),
            NULL // lParam
            );
    if (warningWindow == NULL) {
        //XXX: actually this is bad... We didn't manage to create the window.
        return;
    }

    HICON hIcon = GetSecurityWarningIcon();

    ICONINFO ii;
    ::GetIconInfo(hIcon, &ii);

    //Note: we assume that every security icon has exactly the same shape.
    HRGN rgn = BitmapUtil::BitmapToRgn(ii.hbmColor);
    if (rgn) {
        ::SetWindowRgn(warningWindow, rgn, TRUE);
    }

    // Now we need to create the tooltip control for this window.
    if (!ComCtl32Util::GetInstance().IsToolTipControlInitialized()) {
        return;
    }

    securityTooltipWindow = ::CreateWindowEx(
            WS_EX_TOPMOST,
            TOOLTIPS_CLASS,
            NULL,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,		
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            warningWindow,
            NULL,
            AwtToolkit::GetInstance().GetModuleHandle(),
            NULL
            );

    ::SetWindowPos(securityTooltipWindow,
            HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);


    // We currently don't expect changing the size of the window,
    // hence we may not care of updating the TOOL position/size.
    ::GetClientRect(warningWindow, &rect);

    TOOLINFO ti;

    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = warningWindow;
    ti.hinst = AwtToolkit::GetInstance().GetModuleHandle();
    ti.uId = 0;
    ti.lpszText = warningString;
    ti.rect.left = rect.left;    
    ti.rect.top = rect.top;
    ti.rect.right = rect.right;
    ti.rect.bottom = rect.bottom;

    ::SendMessage(securityTooltipWindow, TTM_ADDTOOL,
            0, (LPARAM) (LPTOOLINFO) &ti);
}

void AwtWindow::DestroyWarningWindow()
{
    if (!IsUntrusted()) {
        return;
    }
    if (--AwtWindow::untrustedWindowsCounter == 0) {
        AwtToolkit::GetInstance().UninstallMouseLowLevelHook();
    }
    if (warningWindow != NULL) {
        // Note that the warningWindow is an owned window, and hence
        // it would be destroyed automatically. However, the window
        // class may only be unregistered if there's no any single
        // window left using this class. Thus, we're destroying the
        // warning window manually. Note that the tooltip window
        // will be destroyed automatically because it's an owned
        // window as well.
        ::DestroyWindow(warningWindow);
        warningWindow = NULL;
        securityTooltipWindow = NULL;
        UnregisterWarningWindowClass();
    }
}

void AwtWindow::DestroyHWnd()
{
    DestroyWarningWindow();
    AwtCanvas::DestroyHWnd();
}

LPCTSTR AwtWindow::GetWarningWindowClassName()
{
    return TEXT("SunAwtWarningWindow");
}

void AwtWindow::FillWarningWindowClassInfo(WNDCLASS *lpwc)
{
    lpwc->style         = 0L;
    lpwc->lpfnWndProc   = (WNDPROC)WarningWindowProc;
    lpwc->cbClsExtra    = 0;
    lpwc->cbWndExtra    = 0;
    lpwc->hInstance     = AwtToolkit::GetInstance().GetModuleHandle(),
    lpwc->hIcon         = AwtToolkit::GetInstance().GetAwtIcon();
    lpwc->hCursor       = ::LoadCursor(NULL, IDC_ARROW);
    lpwc->hbrBackground = NULL;
    lpwc->lpszMenuName  = NULL;
    lpwc->lpszClassName = AwtWindow::GetWarningWindowClassName();
}

void AwtWindow::RegisterWarningWindowClass()
{
    WNDCLASS  wc;  

    ::ZeroMemory(&wc, sizeof(wc));

    if (!::GetClassInfo(AwtToolkit::GetInstance().GetModuleHandle(),
                        AwtWindow::GetWarningWindowClassName(), &wc))
    {
        AwtWindow::FillWarningWindowClassInfo(&wc);
        ATOM atom = ::RegisterClass(&wc);
        DASSERT(atom != 0);
    }
}

void AwtWindow::UnregisterWarningWindowClass()
{
    ::UnregisterClass(AwtWindow::GetWarningWindowClassName(), AwtToolkit::GetInstance().GetModuleHandle());
}

HICON AwtWindow::GetSecurityWarningIcon()
{
    // It is assumed that the icon at index 0 is gray
    const UINT index = securityAnimationKind == akShow ?
        securityWarningAnimationStage : 0;
    HICON ico = AwtToolkit::GetInstance().GetSecurityWarningIcon(index,
            warningWindowWidth, warningWindowHeight);
    return ico;
}

// This function calculates the bounds of the warning window and stores them
// into the RECT structure pointed by the argument rect.
void AwtWindow::CalculateWarningWindowBounds(JNIEnv *env, LPRECT rect)
{
    RECT windowBounds;
    AwtToolkit::GetWindowRect(GetHWnd(), &windowBounds);

    jobject target = GetTarget(env);
    jobject point2D = env->CallObjectMethod(target,
            calculateSecurityWarningPositionMID,
            (jdouble)windowBounds.left, (jdouble)windowBounds.top,
            (jdouble)(windowBounds.right - windowBounds.left),
            (jdouble)(windowBounds.bottom - windowBounds.top));
    env->DeleteLocalRef(target);

    static jclass point2DClassID = NULL;
    static jmethodID point2DGetXMID = NULL;
    static jmethodID point2DGetYMID = NULL;

    if (point2DClassID == NULL) {
        jclass point2DClassIDLocal = env->FindClass("java/awt/geom/Point2D");
        point2DClassID = (jclass)env->NewGlobalRef(point2DClassIDLocal);
        env->DeleteLocalRef(point2DClassIDLocal);
    }

    if (point2DGetXMID == NULL) {
        point2DGetXMID = env->GetMethodID(point2DClassID, "getX", "()D");
    }
    if (point2DGetYMID == NULL) {
        point2DGetYMID = env->GetMethodID(point2DClassID, "getY", "()D");
    }

    int x = (int)env->CallDoubleMethod(point2D, point2DGetXMID);
    int y = (int)env->CallDoubleMethod(point2D, point2DGetYMID);

    env->DeleteLocalRef(point2D);

    rect->left = x;
    rect->top = y;
    rect->right = rect->left + warningWindowWidth;
    rect->bottom = rect->top + warningWindowHeight;
}

LRESULT CALLBACK AwtWindow::WarningWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_PAINT:
            PaintWarningWindow(hwnd);
            return 0;

        case WM_MOUSEACTIVATE:
            {
                // Retrive the owner of the warning window.
                HWND javaWindow = ::GetParent(hwnd);
                if (javaWindow) {
                    // If the window is blocked by a modal dialog, substitute
                    // its handle with the topmost blocker.
                    HWND topmostBlocker = GetTopmostModalBlocker(javaWindow);
                    if (::IsWindow(topmostBlocker)) {
                        javaWindow = topmostBlocker;
                    }

                    ::BringWindowToTop(javaWindow);

                    AwtWindow * window =
                        (AwtWindow*)AwtComponent::GetComponent(javaWindow);
                    if (window == NULL) {
                        // Quite unlikely to go into here, but it's way better
                        // than getting a crash.
                        ::SetForegroundWindow(javaWindow);
                    } else {
                        // Activate the window if it is focusable and inactive
                        if (window->IsFocusableWindow() && 
                                javaWindow != ::GetActiveWindow()) {
                            ::SetForegroundWindow(javaWindow);
                        } else {
                            // ...otherwise just start the animation.
                            window->StartSecurityAnimation(akShow);
                        }
                    }

                    // In every case if there's a top-most blocker, we need to
                    // enable modal animation.
                    if (::IsWindow(topmostBlocker)) {
                        AwtDialog::AnimateModalBlocker(topmostBlocker);
                    }
                }
                return MA_NOACTIVATEANDEAT;
            }
    }
    return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void AwtWindow::PaintWarningWindow(HWND warningWindow)
{
    RECT updateRect;

    if (!::GetUpdateRect(warningWindow, &updateRect, FALSE)) {
        // got nothing to update
        return;
    }

    PAINTSTRUCT ps;
    HDC hdc = ::BeginPaint(warningWindow, &ps);
    if (hdc == NULL) {
        // indicates an error
        return;
    }

    PaintWarningWindow(warningWindow, hdc);

    ::EndPaint(warningWindow, &ps);
}

void AwtWindow::PaintWarningWindow(HWND warningWindow, HDC hdc)
{
    HWND javaWindow = ::GetParent(warningWindow);

    AwtWindow * window = (AwtWindow*)AwtComponent::GetComponent(javaWindow);
    if (window == NULL) {
        return;
    }

    ::DrawIconEx(hdc, 0, 0, window->GetSecurityWarningIcon(),
            window->warningWindowWidth, window->warningWindowHeight,
            0, NULL, DI_NORMAL);
}

static const UINT_PTR IDT_AWT_SECURITYANIMATION = 0x102;

// Approximately 6 times a second. 0.75 seconds total.
static const UINT securityAnimationTimerElapse = 150;
static const UINT securityAnimationMaxIterations = 5;

void AwtWindow::RepaintWarningWindow()
{
    HDC hdc = ::GetDC(warningWindow);
    PaintWarningWindow(warningWindow, hdc);
    ::ReleaseDC(warningWindow, hdc);
}

void AwtWindow::SetLayered(HWND window, bool layered)
{
    const LONG ex_style = ::GetWindowLong(window, GWL_EXSTYLE);
    ::SetWindowLong(window, GWL_EXSTYLE, layered ?
            ex_style | AWT_WS_EX_LAYERED : ex_style & ~AWT_WS_EX_LAYERED);
}

bool AwtWindow::IsLayered(HWND window)
{
    const LONG ex_style = ::GetWindowLong(window, GWL_EXSTYLE);
    return ex_style & AWT_WS_EX_LAYERED;
}

void AwtWindow::StartSecurityAnimation(AnimationKind kind)
{
    if (!IsUntrusted()) {
        return;
    }
    if (warningWindow == NULL) {
        return;
    }

    securityAnimationKind = kind;

    securityWarningAnimationStage = 1;
    ::SetTimer(GetHWnd(), IDT_AWT_SECURITYANIMATION,
            securityAnimationTimerElapse, NULL);

    load_user_procs();
    if (securityAnimationKind == akShow) {
        ::SetWindowPos(warningWindow,
                IsAlwaysOnTop() ? HWND_TOPMOST : GetHWnd(),
                0, 0, 0, 0,
                SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE |
                SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
        
        if (fn_set_layered_window_attributes) {
            (*fn_set_layered_window_attributes)(warningWindow, RGB(0, 0, 0), 
                    0xFF, AWT_LWA_ALPHA);

            AwtWindow::SetLayered(warningWindow, false);
        }
        ::RedrawWindow(warningWindow, NULL, NULL,
                RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    } else if (securityAnimationKind == akPreHide) {
        // Pre-hiding means fading-out. We have to make the window layered.
        // Note: Some VNC clients do not support layered windows, hence
        // we dynamically turn it on and off. See 6805231.
        if (fn_set_layered_window_attributes) {
            AwtWindow::SetLayered(warningWindow, true);
        }
    }

}

void AwtWindow::StopSecurityAnimation()
{
    if (!IsUntrusted()) {
        return;
    }
    if (warningWindow == NULL) {
        return;
    }

    securityWarningAnimationStage = 0;
    ::KillTimer(GetHWnd(), IDT_AWT_SECURITYANIMATION);

    switch (securityAnimationKind) {
        case akHide:
        case akPreHide:
            ::SetWindowPos(warningWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
                    SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE |
                    SWP_HIDEWINDOW | SWP_NOOWNERZORDER);
            break;
        case akShow:
            RepaintWarningWindow();
            break;
    }

    securityAnimationKind = akNone;
}

MsgRouting AwtWindow::WmTimer(UINT_PTR timerID)
{
    if (timerID != IDT_AWT_SECURITYANIMATION) {
        return mrPassAlong;
    }

    if (securityWarningAnimationStage == 0) {
        return mrConsume;
    }

    securityWarningAnimationStage++;
    if (securityWarningAnimationStage >= securityAnimationMaxIterations) {
        if (securityAnimationKind == akPreHide) {
            // chain real hiding
            StartSecurityAnimation(akHide);
        } else {
            StopSecurityAnimation();
        }
    } else {
        switch (securityAnimationKind) {
            case akHide:
                load_user_procs();
                if (fn_set_layered_window_attributes) {
                    BYTE opacity = ((int)0xFF *
                            (securityAnimationMaxIterations -
                             securityWarningAnimationStage)) /
                        securityAnimationMaxIterations;
                    (*fn_set_layered_window_attributes)(warningWindow,
                            RGB(0, 0, 0), opacity, AWT_LWA_ALPHA);
                }
                break;
            case akShow:
            case akNone: // quite unlikely, but quite safe
                RepaintWarningWindow();
                break;
        }
    }

    return mrConsume;
}

// The security warning is visible if:
//    1. The window has the keyboard window focus, OR
//    2. The mouse pointer is located within the window bounds,
//       or within the security warning icon.
void AwtWindow::UpdateSecurityWarningVisibility()
{
    if (!IsUntrusted()) {
        return;
    }
    if (warningWindow == NULL) {
        return;
    }

    bool show = false;

    if (IsVisible() && currentWmSizeState != SIZE_MINIMIZED) {
        if (AwtComponent::GetFocusedWindow() == GetHWnd()) {
            show = true;
        }

        HWND hwnd = AwtToolkit::GetInstance().GetWindowUnderMouse();
        if (hwnd == GetHWnd()) {
            show = true;
        }
        if (hwnd == warningWindow) {
            show = true;
        }
    }

    if (show && (!::IsWindowVisible(warningWindow) ||
                securityAnimationKind == akHide ||
                securityAnimationKind == akPreHide)) {
        StartSecurityAnimation(akShow);
    }
    if (!show && ::IsWindowVisible(warningWindow)) {
        StartSecurityAnimation(akPreHide);
    }
}

void AwtWindow::FocusTransfered(HWND from, HWND to)
{
    AwtWindow * fw = (AwtWindow *)AwtComponent::GetComponent(from);
    AwtWindow * tw = (AwtWindow *)AwtComponent::GetComponent(to);

    if (fw != NULL) {
        fw->UpdateSecurityWarningVisibility();
    }
    if (tw != NULL) {
        tw->UpdateSecurityWarningVisibility();

        // Flash on receiving the keyboard focus even if the warning
        // has already been shown (e.g. by hovering with the mouse)
        tw->StartSecurityAnimation(akShow);
    }
}

void AwtWindow::_RepositionSecurityWarning(void* param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    RepositionSecurityWarningStruct *rsws =
        (RepositionSecurityWarningStruct *)param;
    jobject self = rsws->window;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    AwtWindow *window = (AwtWindow *)pData;

    window->RepositionSecurityWarning(env);

  ret:
    env->DeleteGlobalRef(self);
    delete rsws;
}

/* Create a new AwtWindow object and window.   */
AwtWindow* AwtWindow::Create(jobject self, jobject parent)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject target = NULL;
    AwtWindow* window = NULL;

    try {
        if (env->EnsureLocalCapacity(1) < 0) {
            return NULL;
        }

        AwtWindow* awtParent = NULL;

        PDATA pData;
        if (parent != NULL) {
            JNI_CHECK_PEER_GOTO(parent, done);
            awtParent = (AwtWindow *)pData;
        }

        target = env->GetObjectField(self, AwtObject::targetID);
        JNI_CHECK_NULL_GOTO(target, "null target", done);

        window = new AwtWindow();
      
        {
            if (JNU_IsInstanceOfByName(env, target, "javax/swing/Popup$HeavyWeightWindow") > 0) {
                window->m_isRetainingHierarchyZOrder = TRUE;
            }
            DWORD style = WS_CLIPCHILDREN | WS_POPUP;
            DWORD exStyle = 0;
            if (GetRTL()) {
                exStyle |= WS_EX_RIGHT | WS_EX_LEFTSCROLLBAR;
                if (GetRTLReadingOrder())
                    exStyle |= WS_EX_RTLREADING;
            }
            if (awtParent != NULL) {
                window->InitOwner(awtParent);
            } else {
                // specify WS_EX_TOOLWINDOW to remove parentless windows from taskbar
                exStyle |= WS_EX_TOOLWINDOW;
            }
            window->CreateHWnd(env, L"",
                               style, exStyle,
                               0, 0, 0, 0,
                               (awtParent != NULL) ? awtParent->GetHWnd() : NULL,
                               NULL,
                               ::GetSysColor(COLOR_WINDOWTEXT),
                               ::GetSysColor(COLOR_WINDOW),
                               self);

            jint x = env->GetIntField(target, AwtComponent::xID);
            jint y = env->GetIntField(target, AwtComponent::yID);
            jint width = env->GetIntField(target, AwtComponent::widthID);
            jint height = env->GetIntField(target, AwtComponent::heightID);

            /*
             * Initialize icon as inherited from parent if it exists
             */
            if (parent != NULL) {
                window->m_hIcon = awtParent->GetHIcon();
                window->m_hIconSm = awtParent->GetHIconSm();
                window->m_iconInherited = TRUE;
            }
            window->DoUpdateIcon();
            

            /* 
             * Reshape here instead of during create, so that a WM_NCCALCSIZE
             * is sent. 
             */
            window->Reshape(x, y, width, height);
        }
    } catch (...) {
        env->DeleteLocalRef(target);
        throw;
    }

done:
    env->DeleteLocalRef(target);
    return window;
}

BOOL AwtWindow::IsOneOfOwnersOf(AwtWindow * wnd) {
    while (wnd != NULL) {
        if (wnd == this || wnd->GetOwningFrameOrDialog() == this) return TRUE;
        wnd = (AwtWindow*)GetComponent(::GetWindow(wnd->GetHWnd(), GW_OWNER));
    }
    return FALSE;
}

void AwtWindow::InitOwner(AwtWindow *owner)
{
    DASSERT(owner != NULL);
    while (owner != NULL &&
        _tcscmp(owner->GetClassName(), TEXT("SunAwtWindow")) == 0) {

        HWND ownerOwnerHWND = ::GetWindow(owner->GetHWnd(), GW_OWNER);
        if (ownerOwnerHWND == NULL) {
            owner = NULL;
            break;
        }
        owner = (AwtWindow *)AwtComponent::GetComponent(ownerOwnerHWND);        
    }
    m_owningFrameDialog = (AwtFrame *)owner;
}

//
// Override AwtComponent's Reshape to handle minimized/maximized
// windows, fixes 4065534 (robi.khan@eng).
// NOTE: See also AwtWindow::WmSize
//
void AwtWindow::Reshape(int x, int y, int width, int height) 
{
    HWND    hWndSelf = GetHWnd();

    if ( ::IsIconic(hWndSelf)) {
    // normal AwtComponent::Reshape will not work for iconified windows so...
        WINDOWPLACEMENT wp;
        POINT       ptMinPosition = {x,y};
        POINT       ptMaxPosition = {0,0};
        RECT        rcNormalPosition = {x,y,x+width,y+height};
        RECT        rcWorkspace;
        HWND        hWndDesktop = GetDesktopWindow();

        // SetWindowPlacement takes workspace coordinates, but
        // if taskbar is at top of screen, workspace coords !=
        // screen coords, so offset by workspace origin
        VERIFY(::SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&rcWorkspace, 0));
        ::OffsetRect(&rcNormalPosition, -rcWorkspace.left, -rcWorkspace.top);

        // set the window size for when it is not-iconified
        wp.length = sizeof(wp);
        wp.flags = WPF_SETMINPOSITION;
        wp.showCmd = SW_SHOWNA;
        wp.ptMinPosition = ptMinPosition;
        wp.ptMaxPosition = ptMaxPosition;
        wp.rcNormalPosition = rcNormalPosition;
        SetWindowPlacement(hWndSelf, &wp);
        return;
    }
    
    if (IsZoomed(hWndSelf)) {
    // setting size of maximized window, we remove the
    // maximized state bit (matches Motif behaviour)
    // (calling ShowWindow(SW_RESTORE) would fire an
    //  activation event which we don't want)
        LONG    style = GetStyle();
        DASSERT(style & WS_MAXIMIZE);
        style ^= WS_MAXIMIZE;
        SetStyle(style);
    }

    AwtComponent::Reshape(x, y, width, height);
}

void AwtWindow::moveToDefaultLocation() {
    HWND boggy = ::CreateWindow(GetClassName(), L"BOGGY", WS_OVERLAPPED, CW_USEDEFAULT, 0 ,0, 0, 
        NULL, NULL, NULL, NULL);
    RECT defLoc;
    VERIFY(::GetWindowRect(boggy, &defLoc));
    VERIFY(::DestroyWindow(boggy));
    VERIFY(::SetWindowPos(GetHWnd(), NULL, defLoc.left, defLoc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER));
}

void AwtWindow::Show()
{
    m_visible = true;
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    BOOL  done = false;
    HWND hWnd = GetHWnd();

    if (env->EnsureLocalCapacity(2) < 0) {
        return;
    }
    jobject target = GetTarget(env);
    INT nCmdShow;

    AwtFrame* owningFrame = GetOwningFrameOrDialog();
    if (IsFocusableWindow() && owningFrame != NULL &&
        ::GetForegroundWindow() == owningFrame->GetHWnd())
    {
        nCmdShow = SW_SHOW;
    } else {
        nCmdShow = SW_SHOWNA;
    }

    BOOL locationByPlatform = env->GetBooleanField(GetTarget(env), AwtWindow::locationByPlatformID);

    if (locationByPlatform) {
         moveToDefaultLocation();
    }
    
    // The following block exists to support Menu/Tooltip animation for
    // Swing programs in a way which avoids introducing any new public api into 
    // AWT or Swing.
    // This code should eventually be replaced by a better longterm solution
    // which might involve tagging java.awt.Window instances with a semantic
    // property so platforms can animate/decorate/etc accordingly.
    //
    if ((IS_WIN98 || IS_WIN2000) &&
        JNU_IsInstanceOfByName(env, target, "com/sun/java/swing/plaf/windows/WindowsPopupWindow") > 0) {
        static jfieldID windowTypeFID = NULL;
        jint windowType = 0;
        BOOL  animateflag = FALSE;
        BOOL  fadeflag = FALSE;
            DWORD animateStyle = 0;

        if (windowTypeFID == NULL) {
            // Initialize Window type constants ONCE...

            jfieldID windowTYPESFID[TYPES_COUNT];
            jclass cls = env->GetObjectClass(target);
            windowTypeFID = env->GetFieldID(cls, "windowType", "I");

            windowTYPESFID[UNSPECIFIED] = env->GetStaticFieldID(cls, "UNDEFINED_WINDOW_TYPE", "I");
            windowTYPESFID[TOOLTIP] = env->GetStaticFieldID(cls, "TOOLTIP_WINDOW_TYPE", "I");
            windowTYPESFID[MENU] = env->GetStaticFieldID(cls, "MENU_WINDOW_TYPE", "I");
            windowTYPESFID[SUBMENU] = env->GetStaticFieldID(cls, "SUBMENU_WINDOW_TYPE", "I");
            windowTYPESFID[POPUPMENU] = env->GetStaticFieldID(cls, "POPUPMENU_WINDOW_TYPE", "I");
            windowTYPESFID[COMBOBOX_POPUP] = env->GetStaticFieldID(cls, "COMBOBOX_POPUP_WINDOW_TYPE", "I");

            for (int i=0; i < 6; i++) {
                windowTYPES[i] = env->GetStaticIntField(cls, windowTYPESFID[i]);
            }
            env->DeleteLocalRef(cls);
        }
        windowType = env->GetIntField(target, windowTypeFID);

        if (windowType == windowTYPES[TOOLTIP]) {
            if (IS_WIN2000) {
                SystemParametersInfo(SPI_GETTOOLTIPANIMATION, 0, &animateflag, 0);
                SystemParametersInfo(SPI_GETTOOLTIPFADE, 0, &fadeflag, 0);
            } else {
                // use same setting as menus
                SystemParametersInfo(SPI_GETMENUANIMATION, 0, &animateflag, 0);
            }
            if (animateflag) {
              // AW_BLEND currently produces runtime parameter error
              // animateStyle = fadeflag? AW_BLEND : AW_SLIDE | AW_VER_POSITIVE;
                 animateStyle = fadeflag? 0 : AW_SLIDE | AW_VER_POSITIVE;
            }
        } else if (windowType == windowTYPES[MENU] || windowType == windowTYPES[SUBMENU] || 
                   windowType == windowTYPES[POPUPMENU]) {
            SystemParametersInfo(SPI_GETMENUANIMATION, 0, &animateflag, 0);
            if (animateflag) {

                if (IS_WIN2000) {
                    SystemParametersInfo(SPI_GETMENUFADE, 0, &fadeflag, 0);
                    if (fadeflag) {
                      // AW_BLEND currently produces runtime parameter error
                      //animateStyle = AW_BLEND;                      
                    } 
                }
                if (animateStyle == 0 && !fadeflag) {
                    animateStyle = AW_SLIDE;
                    if (windowType == windowTYPES[MENU]) {
                      animateStyle |= AW_VER_POSITIVE;
                    } else if (windowType == windowTYPES[SUBMENU]) {
                      animateStyle |= AW_HOR_POSITIVE;
                    } else { /* POPUPMENU */
                      animateStyle |= (AW_VER_POSITIVE | AW_HOR_POSITIVE);
                    }
                }
            }
        } else if (windowType == windowTYPES[COMBOBOX_POPUP]) {
            SystemParametersInfo(SPI_GETCOMBOBOXANIMATION, 0, &animateflag, 0);
            if (animateflag) {
                 animateStyle = AW_SLIDE | AW_VER_POSITIVE;
            }
        } 

        if (animateStyle != 0) {
            load_user_procs();

            if (fn_animate_window != NULL) {
                BOOL result = (*fn_animate_window)(hWnd, (DWORD)200, animateStyle);
                if (result == 0) {
                    LPTSTR      msgBuffer = NULL;
                    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      GetLastError(),
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR)&msgBuffer, // it's an output parameter when allocate buffer is used
                      0,
                      NULL);

                    if (msgBuffer == NULL) {
                        msgBuffer = TEXT("<Could not get GetLastError() message text>");
                    }
                    _ftprintf(stderr,TEXT("AwtWindow::Show: AnimateWindow: "));
                    _ftprintf(stderr,msgBuffer);
                    LocalFree(msgBuffer);
                } else {
                  // WM_PAINT is not automatically sent when invoking AnimateWindow,
                  // so force an expose event
                    RECT rect;
                    ::GetWindowRect(hWnd,&rect);
                    ::ScreenToClient(hWnd, (LPPOINT)&rect);
                    ::InvalidateRect(hWnd,&rect,TRUE);
                    ::UpdateWindow(hWnd); 
                    done = TRUE;
                }
            }
        } 
    }
    if (!done) {
        // transient windows shouldn't change the owner window's position in the z-order
        if (IsRetainingHierarchyZOrder()){
            UINT flags = SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_NOOWNERZORDER;
            if (nCmdShow == SW_SHOWNA) {
                flags |= SWP_NOACTIVATE;
            }
            ::SetWindowPos(GetHWnd(), HWND_TOPMOST, 0, 0, 0, 0, flags);
        } else {
            ::ShowWindow(GetHWnd(), nCmdShow);
        }
    }
    env->DeleteLocalRef(target);
}

/*
 * Get and return the insets for this window (container, really).
 * Calculate & cache them while we're at it, for use by AwtGraphics
 */
BOOL AwtWindow::UpdateInsets(jobject insets)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    DASSERT(GetPeer(env) != NULL);
    if (env->EnsureLocalCapacity(2) < 0) {
        return FALSE;
    }

    // fix 4167248 : don't update insets when frame is iconified
    // to avoid bizarre window/client rectangles
    if (::IsIconic(GetHWnd())) {
        return FALSE;
    }

    /*
     * Code to calculate insets. Stores results in frame's data
     * members, and in the peer's Inset object.
     */
    RECT outside;
    RECT inside;
    int extraBottomInsets = 0;

    ::GetClientRect(GetHWnd(), &inside);
    ::GetWindowRect(GetHWnd(), &outside);

    /* Update our inset member */
    if (outside.right - outside.left > 0 && outside.bottom - outside.top > 0) {
        ::MapWindowPoints(GetHWnd(), 0, (LPPOINT)&inside, 2);
        m_insets.top = inside.top - outside.top;
        m_insets.bottom = outside.bottom - inside.bottom + extraBottomInsets;
        m_insets.left = inside.left - outside.left;
        m_insets.right = outside.right - inside.right;
    } else {
        m_insets.top = -1;
    }
    if (m_insets.left < 0 || m_insets.top < 0 ||
        m_insets.right < 0 || m_insets.bottom < 0)
    {
        /* This window hasn't been sized yet -- use system metrics. */
        jobject target = GetTarget(env);
        if (IsUndecorated() == FALSE) {
            /* Get outer frame sizes. */
            LONG style = GetStyle();
            if (style & WS_THICKFRAME) {
                m_insets.left = m_insets.right = 
                    ::GetSystemMetrics(SM_CXSIZEFRAME);
                m_insets.top = m_insets.bottom = 
                    ::GetSystemMetrics(SM_CYSIZEFRAME);
            } else {
                m_insets.left = m_insets.right = 
                    ::GetSystemMetrics(SM_CXDLGFRAME);
                m_insets.top = m_insets.bottom = 
                    ::GetSystemMetrics(SM_CYDLGFRAME);
            }
          

            /* Add in title. */
            m_insets.top += ::GetSystemMetrics(SM_CYCAPTION);
        }
        else { 
            /* fix for 4418125: Undecorated frames are off by one */
            /* undo the -1 set above */
            /* Additional fix for 5059656 */
	        /* Also, 5089312: Window insets should be 0. */	 
            ::memset(&m_insets, 0, sizeof(m_insets));
        }

        /* Add in menuBar, if any. */
        if (JNU_IsInstanceOfByName(env, target, "java/awt/Frame") > 0 &&
            ((AwtFrame*)this)->GetMenuBar()) {
            m_insets.top += ::GetSystemMetrics(SM_CYMENU);
        }
        m_insets.bottom += extraBottomInsets;
        env->DeleteLocalRef(target);
    }

    BOOL insetsChanged = FALSE;

    jobject peer = GetPeer(env);
    /* Get insets into our peer directly */
    jobject peerInsets = (env)->GetObjectField(peer, AwtPanel::insets_ID);
    DASSERT(!safe_ExceptionOccurred(env));
    if (peerInsets != NULL) { // may have been called during creation
        (env)->SetIntField(peerInsets, AwtInsets::topID, m_insets.top);
        (env)->SetIntField(peerInsets, AwtInsets::bottomID, 
                           m_insets.bottom);
        (env)->SetIntField(peerInsets, AwtInsets::leftID, m_insets.left);
        (env)->SetIntField(peerInsets, AwtInsets::rightID, m_insets.right);
    }
    /* Get insets into the Inset object (if any) that was passed */
    if (insets != NULL) {
        (env)->SetIntField(insets, AwtInsets::topID, m_insets.top);
        (env)->SetIntField(insets, AwtInsets::bottomID, m_insets.bottom);
        (env)->SetIntField(insets, AwtInsets::leftID, m_insets.left);
        (env)->SetIntField(insets, AwtInsets::rightID, m_insets.right);
    }
    env->DeleteLocalRef(peerInsets);

    insetsChanged = !::EqualRect( &m_old_insets, &m_insets );       
    ::CopyRect( &m_old_insets, &m_insets );
    
    // REMIND: placeholder for future acceleration-related activity
    if (insetsChanged) {
        // Since insets are changed we need to update the surfaceData object
        // to reflect that change
        env->CallVoidMethod(peer, AwtComponent::replaceSurfaceDataLaterMID);
    }

    return insetsChanged;
}

/**
 * Sometimes we need the hWnd that actually owns this Window's hWnd (if
 * there is an owner).
 */
HWND AwtWindow::GetTopLevelHWnd()
{
    return m_owningFrameDialog ? m_owningFrameDialog->GetHWnd() :
                                 GetHWnd();
}

/*
 * Although this function sends ComponentEvents, it needs to be defined
 * here because only top-level windows need to have move and resize
 * events fired from native code.  All contained windows have these events
 * fired from common Java code.
 */
void AwtWindow::SendComponentEvent(jint eventId)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    static jclass classEvent = NULL;
    if (classEvent == NULL) {
        if (env->PushLocalFrame(1) < 0)
            return;
        classEvent = env->FindClass("java/awt/event/ComponentEvent");
        if (classEvent != NULL) {
            classEvent = (jclass)env->NewGlobalRef(classEvent);
        }
        env->PopLocalFrame(0);
    }
    static jmethodID eventInitMID = NULL;
    if (eventInitMID == NULL) {
        eventInitMID = env->GetMethodID(classEvent, "<init>",
                                        "(Ljava/awt/Component;I)V");
        if (eventInitMID == NULL) {
            return;
        }
    }
    if (env->EnsureLocalCapacity(2) < 0) {
        return;
    }
    jobject target = GetTarget(env);
    jobject event = env->NewObject(classEvent, eventInitMID, 
                                   target, eventId);
    DASSERT(!safe_ExceptionOccurred(env));
    DASSERT(event != NULL);
    SendEvent(event);

    env->DeleteLocalRef(target);
    env->DeleteLocalRef(event);
}

void AwtWindow::SendWindowEvent(jint id, HWND opposite,
                                jint oldState, jint newState)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    static jclass wClassEvent;
    if (wClassEvent == NULL) {
        if (env->PushLocalFrame(1) < 0)
            return;
        wClassEvent = env->FindClass("java/awt/event/WindowEvent");
        if (wClassEvent != NULL) {
            wClassEvent = (jclass)env->NewGlobalRef(wClassEvent);
        }
        env->PopLocalFrame(0);
        if (wClassEvent == NULL) {
            return;
        }
    }

    static jmethodID wEventInitMID;
    if (wEventInitMID == NULL) {
        wEventInitMID =
            env->GetMethodID(wClassEvent, "<init>",
                             "(Ljava/awt/Window;ILjava/awt/Window;II)V");
        DASSERT(wEventInitMID);
        if (wEventInitMID == NULL) {
            return;
        }
    }

    static jclass sequencedEventCls;
    if (sequencedEventCls == NULL) {
        jclass sequencedEventClsLocal
            = env->FindClass("java/awt/SequencedEvent");
        DASSERT(sequencedEventClsLocal);
        if (sequencedEventClsLocal == NULL) {
            /* exception already thrown */
            return;
        }
        sequencedEventCls =
            (jclass)env->NewGlobalRef(sequencedEventClsLocal);
        env->DeleteLocalRef(sequencedEventClsLocal);
    }

    static jmethodID sequencedEventConst;
    if (sequencedEventConst == NULL) {
        sequencedEventConst =
            env->GetMethodID(sequencedEventCls, "<init>",
                             "(Ljava/awt/AWTEvent;)V");
    }

    if (env->EnsureLocalCapacity(3) < 0) {
        return;
    }

    jobject target = GetTarget(env);
    jobject jOpposite = NULL;
    if (opposite != NULL) {
        AwtComponent *awtOpposite = AwtComponent::GetComponent(opposite);
        if (awtOpposite != NULL) {
            jOpposite = awtOpposite->GetTarget(env);
        }
    }
    jobject event = env->NewObject(wClassEvent, wEventInitMID, target, id,
                                   jOpposite, oldState, newState);
    DASSERT(!safe_ExceptionOccurred(env));
    DASSERT(event != NULL);
    if (jOpposite != NULL) {
        env->DeleteLocalRef(jOpposite); jOpposite = NULL;
    }
    env->DeleteLocalRef(target); target = NULL;

    if (id == java_awt_event_WindowEvent_WINDOW_GAINED_FOCUS ||
        id == java_awt_event_WindowEvent_WINDOW_LOST_FOCUS)
    {
        jobject sequencedEvent = env->NewObject(sequencedEventCls,
                                                sequencedEventConst,
                                                event);
        DASSERT(!safe_ExceptionOccurred(env));
        DASSERT(sequencedEvent != NULL);
        env->DeleteLocalRef(event);
        event = sequencedEvent;
    }

    SendEvent(event);

    env->DeleteLocalRef(event);
}

MsgRouting AwtWindow::WmActivate(UINT nState, BOOL fMinimized, HWND opposite)
{
    jint type;

    if (nState != WA_INACTIVE) {
        ::SetFocus((sm_focusOwner == NULL ||
                    AwtComponent::GetTopLevelParentForWindow(sm_focusOwner) !=
                    GetHWnd()) ? NULL : sm_focusOwner);
        type = java_awt_event_WindowEvent_WINDOW_GAINED_FOCUS;
        AwtToolkit::GetInstance().
            InvokeFunctionLater(BounceActivation, this);
        AwtComponent::SetFocusedWindow(GetHWnd());
    } else {
        if (m_grabbedWindow != NULL && !m_grabbedWindow->IsOneOfOwnersOf(this)) {
            m_grabbedWindow->Ungrab();
        }
        type = java_awt_event_WindowEvent_WINDOW_LOST_FOCUS;
        AwtComponent::SetFocusedWindow(NULL);
    }

    SendWindowEvent(type, opposite);
    return mrConsume;
}

void AwtWindow::BounceActivation(void *self) {
    AwtWindow *wSelf = (AwtWindow *)self;

    if (::GetActiveWindow() == wSelf->GetHWnd()) {
        AwtFrame *owner = wSelf->GetOwningFrameOrDialog();

        if (owner != NULL) {
            sm_suppressFocusAndActivation = TRUE;
            ::SetActiveWindow(owner->GetHWnd());
            ::SetFocus(owner->GetProxyFocusOwner());
            sm_suppressFocusAndActivation = FALSE;
        }
    }
}

MsgRouting AwtWindow::WmCreate()
{
    return mrDoDefault;  
}

MsgRouting AwtWindow::WmClose()
{
    SendWindowEvent(java_awt_event_WindowEvent_WINDOW_CLOSING);

    /* Rely on above notification to handle quitting as needed */
    return mrConsume;  
}

MsgRouting AwtWindow::WmDestroy()
{
    SendWindowEvent(java_awt_event_WindowEvent_WINDOW_CLOSED);
    return AwtComponent::WmDestroy();
}

MsgRouting AwtWindow::WmShowWindow(BOOL show, UINT status)
{
    /*
     * Original fix for 4810575. Modified for 6386592.
     * If an owned window (not frame/dialog) gets disposed we should synthesize
     * WM_ACTIVATE for its nearest owner. This is not performed by default because
     * the owner frame/dialog is natively active.
     */
    HWND hwndSelf = GetHWnd();
    HWND hwndParent = ::GetParent(hwndSelf);

    if (!show && IsSimpleWindow() && hwndSelf == AwtComponent::GetFocusedWindow() &&
        hwndParent != NULL && ::IsWindowVisible(hwndParent))
    {
        ::PostMessage(hwndParent, WM_ACTIVATE, (WPARAM)WA_ACTIVE, (LPARAM)hwndSelf);
    }

    //Fixed 4842599: REGRESSION: JPopupMenu not Hidden Properly After Iconified and Deiconified 
    if (show && (status == SW_PARENTOPENING)) {
        if (!IsVisible()) {
            return mrConsume;
        }
    }

    return AwtCanvas::WmShowWindow(show, status);
}

/*
 * Override AwtComponent's move handling to first update the
 * java AWT target's position fields directly, since Windows
 * and below can be resized from outside of java (by user)
 */
MsgRouting AwtWindow::WmMove(int x, int y)
{
    if ( ::IsIconic(GetHWnd()) ) {
    // fixes 4065534 (robi.khan@eng)
    // if a window is iconified we don't want to update
    // it's target's position since minimized Win32 windows
    // move to -32000, -32000 for whatever reason
    // NOTE: See also AwtWindow::Reshape
        return mrDoDefault;
    }

    if (m_screenNum == -1) {
    // Set initial value
        m_screenNum = GetScreenImOn();
    } 
    else {
        CheckIfOnNewScreen();
    }

    /* Update the java AWT target component's fields directly */
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    if (env->EnsureLocalCapacity(1) < 0) {
        return mrConsume;
    }
    jobject peer = GetPeer(env);
    jobject target = env->GetObjectField(peer, AwtObject::targetID);

    RECT rect;
    ::GetWindowRect(GetHWnd(), &rect);

    (env)->SetIntField(target, AwtComponent::xID, rect.left);
    (env)->SetIntField(target, AwtComponent::yID, rect.top);
    (env)->SetIntField(peer, AwtWindow::sysXID, rect.left);
    (env)->SetIntField(peer, AwtWindow::sysYID, rect.top);
    SendComponentEvent(java_awt_event_ComponentEvent_COMPONENT_MOVED);

    env->DeleteLocalRef(target);
    return AwtComponent::WmMove(x, y);
}

MsgRouting AwtWindow::WmGetMinMaxInfo(LPMINMAXINFO lpmmi)
{
    MsgRouting r = AwtCanvas::WmGetMinMaxInfo(lpmmi);
    if ((m_minSize.x == 0) && (m_minSize.y == 0)) {
        return r;
    }
    lpmmi->ptMinTrackSize.x = m_minSize.x;
    lpmmi->ptMinTrackSize.y = m_minSize.y;
    return mrConsume;
}

/*
 * Override AwtComponent's size handling to first update the 
 * java AWT target's dimension fields directly, since Windows
 * and below can be resized from outside of java (by user)
 */
MsgRouting AwtWindow::WmSize(UINT type, int w, int h)
{
    currentWmSizeState = type;

    if (type == SIZE_MINIMIZED) {
        UpdateSecurityWarningVisibility();
        return mrDoDefault;
    }

    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    if (env->EnsureLocalCapacity(1) < 0)
        return mrDoDefault;
    jobject target = GetTarget(env);
    // fix 4167248 : ensure the insets are up-to-date before using
    BOOL insetsChanged = UpdateInsets(NULL);
    int newWidth = w + m_insets.left + m_insets.right;
    int newHeight = h + m_insets.top + m_insets.bottom;

    (env)->SetIntField(target, AwtComponent::widthID, newWidth);
    (env)->SetIntField(target, AwtComponent::heightID, newHeight);
    
    jobject peer = GetPeer(env);
    (env)->SetIntField(peer, AwtWindow::sysWID, newWidth);
    (env)->SetIntField(peer, AwtWindow::sysHID, newHeight);

    if (!m_resizing) {
        WindowResized();
    }

    env->DeleteLocalRef(target);
    return AwtComponent::WmSize(type, w, h);
}

MsgRouting AwtWindow::WmPaint(HDC)
{
    PaintUpdateRgn(&m_insets);
    return mrConsume;
}

MsgRouting AwtWindow::WmSysCommand(UINT uCmdType, int xPos, int yPos)
{
    if (uCmdType == SC_SIZE) {
        m_resizing = TRUE;
    }
    return mrDoDefault;
}

MsgRouting AwtWindow::WmExitSizeMove()
{
    if (m_resizing) {
        WindowResized();
        m_resizing = FALSE;
    }
    return mrDoDefault;
}

MsgRouting AwtWindow::WmSettingChange(UINT wFlag, LPCTSTR pszSection)
{
    if (wFlag == SPI_SETNONCLIENTMETRICS) {
    // user changed window metrics in
    // Control Panel->Display->Appearance
    // which may cause window insets to change
        UpdateInsets(NULL);
        
    // [rray] fix for 4407329 - Changing Active Window Border width in display 
    //  settings causes problems
        WindowResized();
        Invalidate(NULL);

        return mrConsume;
    }
    return mrDoDefault;
}

MsgRouting AwtWindow::WmNcCalcSize(BOOL fCalcValidRects, 
                                   LPNCCALCSIZE_PARAMS lpncsp, LRESULT& retVal)
{
    MsgRouting mrRetVal = mrDoDefault;

    if (fCalcValidRects == FALSE) {
        return mrDoDefault;
    }
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    if (env->EnsureLocalCapacity(2) < 0) {
        return mrConsume;
    }
    // WM_NCCALCSIZE is usually in response to a resize, but
    // also can be triggered by SetWindowPos(SWP_FRAMECHANGED),
    // which means the insets will have changed - rnk 4/7/1998
    retVal = static_cast<UINT>(DefWindowProc(
                WM_NCCALCSIZE, fCalcValidRects, reinterpret_cast<LPARAM>(lpncsp)));
    if (HasValidRect()) {
        UpdateInsets(NULL);
    }
    mrRetVal = mrConsume;
    return mrRetVal;
}

MsgRouting AwtWindow::WmNcHitTest(UINT x, UINT y, LRESULT& retVal)
{
    // If this window is blocked by modal dialog, return HTCLIENT for any point of it.
    // That prevents it to be moved or resized using the mouse. Disabling these
    // actions to be launched from sysmenu is implemented by ignoring WM_SYSCOMMAND
    if (::IsWindow(GetModalBlocker(GetHWnd()))) {
        retVal = HTCLIENT;
    } else {
        retVal = DefWindowProc(WM_NCHITTEST, 0, MAKELPARAM(x, y));
    }
    return mrConsume;
}

MsgRouting AwtWindow::WmGetIcon(WPARAM iconType, LRESULT& retValue)
{
    return mrDoDefault;
}

LRESULT AwtWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    MsgRouting mr = mrDoDefault;
    LRESULT retValue = 0L;

    switch(message) {
        case WM_GETICON:
            mr = WmGetIcon(wParam, retValue);
            break;
    }

    if (mr != mrConsume) {
        retValue = AwtCanvas::WindowProc(message, wParam, lParam);
    }
    return retValue;
}

/*
 * Fix for BugTraq ID 4041703: keyDown not being invoked.
 * This method overrides AwtCanvas::HandleEvent() since 
 * an empty Window always receives the focus on the activation
 * so we don't have to modify the behavior.
 */
MsgRouting AwtWindow::HandleEvent(MSG *msg, BOOL synthetic)
{
    return AwtComponent::HandleEvent(msg, synthetic);
}

void AwtWindow::WindowResized()
{
    SendComponentEvent(java_awt_event_ComponentEvent_COMPONENT_RESIZED);
    // Need to replace surfaceData on resize to catch changes to 
    // various component-related values, such as insets
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    env->CallVoidMethod(m_peerObject, AwtComponent::replaceSurfaceDataLaterMID);
}

BOOL CALLBACK InvalidateChildRect(HWND hWnd, LPARAM)
{
    TRY;

    ::InvalidateRect(hWnd, NULL, TRUE);
    return TRUE;

    CATCH_BAD_ALLOC_RET(FALSE);
}

void AwtWindow::Invalidate(RECT* r)
{
    ::InvalidateRect(GetHWnd(), NULL, TRUE);
    ::EnumChildWindows(GetHWnd(), (WNDENUMPROC)InvalidateChildRect, 0);
}

BOOL AwtWindow::IsResizable() {
    return m_isResizable;
}

void AwtWindow::SetResizable(BOOL isResizable)
{
    m_isResizable = isResizable;
    if (IsEmbeddedFrame()) {
        return;
    }
    LONG style = GetStyle();
    LONG resizeStyle = WS_MAXIMIZEBOX;

    if (IsUndecorated() == FALSE) {
        resizeStyle |= WS_THICKFRAME;
    }

    if (isResizable) {
        style |= resizeStyle;
    } else {
        style &= ~resizeStyle;
    }
    SetStyle(style);
    RedrawNonClient();
}

// SetWindowPos flags to cause frame edge to be recalculated
static const UINT SwpFrameChangeFlags =
    SWP_FRAMECHANGED | /* causes WM_NCCALCSIZE to be called */
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
    SWP_NOACTIVATE | SWP_NOCOPYBITS | 
    SWP_NOREPOSITION | SWP_NOSENDCHANGING;

//
// Forces WM_NCCALCSIZE to be called to recalculate
// window border (updates insets) without redrawing it
//
void AwtWindow::RecalcNonClient()
{
    ::SetWindowPos(GetHWnd(), (HWND) NULL, 0, 0, 0, 0, SwpFrameChangeFlags|SWP_NOREDRAW);
}

//
// Forces WM_NCCALCSIZE to be called to recalculate
// window border (updates insets) and redraws border to match
//
void AwtWindow::RedrawNonClient()
{
    ::SetWindowPos(GetHWnd(), (HWND) NULL, 0, 0, 0, 0, SwpFrameChangeFlags|SWP_ASYNCWINDOWPOS);
}

int AwtWindow::GetScreenImOn() {
    MHND hmon;
    int scrnNum;

    hmon = ::MonitorFromWindow(GetHWnd(), MONITOR_DEFAULT_TO_PRIMARY);
    DASSERT(hmon != NULL);
    
    scrnNum = AwtWin32GraphicsDevice::GetScreenFromMHND(hmon);
    DASSERT(scrnNum > -1);

    return scrnNum;
}

/* Check to see if we've been moved onto another screen.
 * If so, update internal data, surfaces, etc.
 */

void AwtWindow::CheckIfOnNewScreen() {
    int curScrn = GetScreenImOn();

    if (curScrn != m_screenNum) {  // we've been moved 
        JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

        jclass peerCls = env->GetObjectClass(m_peerObject);
        DASSERT(peerCls);

        jmethodID draggedID = env->GetMethodID(peerCls, "draggedToNewScreen",
                                               "()V");
        DASSERT(draggedID);

        env->CallVoidMethod(m_peerObject, draggedID);
        m_screenNum = curScrn;

        env->DeleteLocalRef(peerCls);
    }
}

BOOL AwtWindow::IsFocusableWindow() {
    /*
     * For Window/Frame/Dialog to accept focus it should:
     * - be focusable;
     * - be not blocked by any modal blocker.
     */
    BOOL focusable = m_isFocusableWindow && !::IsWindow(AwtWindow::GetModalBlocker(GetHWnd()));
    AwtFrame *owner = GetOwningFrameOrDialog(); // NULL for Frame and Dialog

    if (owner != NULL) {
        /*
         * Also for Window (not Frame/Dialog) to accept focus:
         * - its decorated parent should accept focus;
         */
        focusable = focusable && owner->IsFocusableWindow();
    }
    return focusable;
}

void AwtWindow::SetModalBlocker(HWND window, HWND blocker) {
    if (!::IsWindow(window)) {
        return;
    }
    if (::IsWindow(blocker)) {
        ::SetProp(window, ModalBlockerProp, reinterpret_cast<HANDLE>(blocker));
        ::EnableWindow(window, FALSE);
    } else {
        ::RemoveProp(window, ModalBlockerProp);
        AwtComponent *comp = AwtComponent::GetComponent(window);
        // we don't expect to be called with non-java HWNDs
        DASSERT(comp && comp->IsTopLevel());
        // we should not unblock disabled toplevels
        ::EnableWindow(window, comp->isEnabled());
    }
}

void AwtWindow::SetAndActivateModalBlocker(HWND window, HWND blocker) {
    if (!::IsWindow(window)) { 
        return; 
    } 
    AwtWindow::SetModalBlocker(window, blocker); 
    if (::IsWindow(blocker)) {
        // We must check for visibility. Otherwise invisible dialog will receive WM_ACTIVATE. 
        if (::IsWindowVisible(blocker)) { 
            ::BringWindowToTop(blocker);
            ::SetForegroundWindow(blocker);
        }
    }
}

HWND AwtWindow::GetTopmostModalBlocker(HWND window)
{
    HWND ret, blocker = NULL;

    do {
        ret = blocker;
        blocker = AwtWindow::GetModalBlocker(window);
        window = blocker;
    } while (::IsWindow(blocker));

    return ret;
}

void AwtWindow::FlashWindowEx(HWND hWnd, UINT count, DWORD timeout, DWORD flags) {
    FLASHWINFO fi;
    fi.cbSize = sizeof(fi);
    fi.hwnd = hWnd;
    fi.dwFlags = flags;
    fi.uCount = count;
    fi.dwTimeout = timeout;
    ::FlashWindowEx(&fi);
}

void AwtWindow::_ToFront(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject self = (jobject)param;

    AwtWindow *w = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    w = (AwtWindow *)pData;
    if (::IsWindow(w->GetHWnd()))
    {
        UINT flags = SWP_NOMOVE|SWP_NOSIZE;
        BOOL focusable = w->IsFocusableWindow();
        if (!focusable)
        {
            flags = flags|SWP_NOACTIVATE;
        }
        ::SetWindowPos(w->GetHWnd(), HWND_TOP, 0, 0, 0, 0, flags);
        if (focusable)
        {
            ::SetForegroundWindow(w->GetHWnd());
        }
    }
ret:
    env->DeleteGlobalRef(self);
}

void AwtWindow::_ToBack(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject self = (jobject)param;

    AwtWindow *w = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    w = (AwtWindow *)pData;
    if (::IsWindow(w->GetHWnd()))
    {
        HWND hwnd = w->GetHWnd();
//        if (AwtComponent::GetComponent(hwnd) == NULL) {
//            // Window was disposed. Don't bother.
//            return;
//        }

        ::SetWindowPos(hwnd, HWND_BOTTOM, 0, 0 ,0, 0,
            SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);

        // If hwnd is the foreground window or if *any* of its owners are, then
        // we have to reset the foreground window. The reason is that when we send
        // hwnd to back, all of its owners are sent to back as well. If any one of
        // them is the foreground window, then it's possible that we could end up
        // with a foreground window behind a window of another application.
        HWND foregroundWindow = ::GetForegroundWindow();
        BOOL adjustForegroundWindow;
        HWND toTest = hwnd;
        do
        {
            adjustForegroundWindow = (toTest == foregroundWindow);
            if (adjustForegroundWindow)
            {
                break;
            }
            toTest = ::GetWindow(toTest, GW_OWNER);
        }
        while (toTest != NULL);

        if (adjustForegroundWindow)
        {
            HWND foregroundSearch = hwnd, newForegroundWindow = NULL;
                while (1)
                {
                foregroundSearch = ::GetNextWindow(foregroundSearch, GW_HWNDPREV);
                if (foregroundSearch == NULL)
                {
                    break;
                }
                LONG style = static_cast<LONG>(::GetWindowLongPtr(foregroundSearch, GWL_STYLE));
                if ((style & WS_CHILD) || !(style & WS_VISIBLE))
                {
                    continue;
                }

                AwtComponent *c = AwtComponent::GetComponent(foregroundSearch);
                if ((c != NULL) && !::IsWindow(AwtWindow::GetModalBlocker(c->GetHWnd())))
                {
                    newForegroundWindow = foregroundSearch;
                }
            }
            if (newForegroundWindow != NULL)
            {
                ::SetWindowPos(newForegroundWindow, HWND_TOP, 0, 0, 0, 0,
                    SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
                if (((AwtWindow*)AwtComponent::GetComponent(newForegroundWindow))->IsFocusableWindow())
                {
                    ::SetForegroundWindow(newForegroundWindow);
                }
            }
            else
            {
                // We *have* to set the active HWND to something new. We simply
                // cannot risk having an active Java HWND which is behind an HWND
                // of a native application. This really violates the Windows user
                // experience.
                //
                // Windows won't allow us to set the foreground window to NULL,
                // so we use the desktop window instead. To the user, it appears
                // that there is no foreground window system-wide.
                ::SetForegroundWindow(::GetDesktopWindow());
            }
        }
    }
ret:
    env->DeleteGlobalRef(self);
}

void AwtWindow::_SetAlwaysOnTop(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SetAlwaysOnTopStruct *sas = (SetAlwaysOnTopStruct *)param;
    jobject self = sas->window;
    jboolean value = sas->value;

    AwtWindow *w = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    w = (AwtWindow *)pData;
    if (::IsWindow(w->GetHWnd()))
    {
        w->SendMessage(WM_AWT_SETALWAYSONTOP, (WPARAM)value, (LPARAM)w);
        w->m_alwaysOnTop = (bool)value;
    }
ret:
    env->DeleteGlobalRef(self);

    delete sas;
}

void AwtWindow::_SetTitle(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SetTitleStruct *sts = (SetTitleStruct *)param;
    jobject self = sts->window;
    jstring title = sts->title;

    AwtWindow *w = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    JNI_CHECK_NULL_GOTO(title, "null title", ret);

    w = (AwtWindow *)pData;
    if (::IsWindow(w->GetHWnd()))
    {
        int length = env->GetStringLength(title);
        WCHAR *buffer = new WCHAR[length + 1];
        env->GetStringRegion(title, 0, length, buffer);
        buffer[length] = L'\0';
        VERIFY(::SetWindowTextW(w->GetHWnd(), buffer));
        delete[] buffer;
    }
ret:
    env->DeleteGlobalRef(self);
    if (title != NULL) {
        env->DeleteGlobalRef(title);
    }

    delete sts;
}

void AwtWindow::_SetResizable(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SetResizableStruct *srs = (SetResizableStruct *)param;
    jobject self = srs->window;
    jboolean resizable = srs->resizable;

    AwtWindow *w = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    w = (AwtWindow *)pData;
    if (::IsWindow(w->GetHWnd()))
    {
        DASSERT(!IsBadReadPtr(w, sizeof(AwtWindow)));
        w->SetResizable(resizable != 0);
    }
ret:
    env->DeleteGlobalRef(self);

    delete srs;
}

void AwtWindow::_UpdateInsets(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    UpdateInsetsStruct *uis = (UpdateInsetsStruct *)param;
    jobject self = uis->window;
    jobject insets = uis->insets;

    AwtWindow *w = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    JNI_CHECK_NULL_GOTO(insets, "null insets", ret);
    w = (AwtWindow *)pData;
    if (::IsWindow(w->GetHWnd()))
    {
        w->UpdateInsets(insets);
    }
ret:
    env->DeleteGlobalRef(self);
    env->DeleteGlobalRef(insets);

    delete uis;
}

void AwtWindow::_ReshapeFrame(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    
    ReshapeFrameStruct *rfs = (ReshapeFrameStruct *)param;
    jobject self = rfs->frame;
    jint x = rfs->x;
    jint y = rfs->y;
    jint w = rfs->w;
    jint h = rfs->h;

    if (env->EnsureLocalCapacity(1) < 0)
    {
        env->DeleteGlobalRef(self);
        delete rfs;
        return;
    }

    AwtFrame *p = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    p = (AwtFrame *)pData;
    if (::IsWindow(p->GetHWnd()))
    {
        jobject target = env->GetObjectField(self, AwtObject::targetID);
        if (target != NULL)        
        {
            // enforce tresholds before sending the event
            // Fix for 4459064 : do not enforce thresholds for embedded frames
            if (!p->IsEmbeddedFrame()) 
            {
                jobject peer = p->GetPeer(env);
                int minWidth = ::GetSystemMetrics(SM_CXMIN);
                int minHeight = ::GetSystemMetrics(SM_CYMIN);
                if (w < minWidth)
                {
                    env->SetIntField(target, AwtComponent::widthID,
                        w = minWidth);
                    env->SetIntField(peer, AwtWindow::sysWID,
                        w);
                }
                if (h < minHeight)
                {
                    env->SetIntField(target, AwtComponent::heightID,
                        h = minHeight);
                    env->SetIntField(peer, AwtWindow::sysHID,
                        h);
                }
            }
            env->DeleteLocalRef(target);

            RECT *r = new RECT;
            ::SetRect(r, x, y, x + w, y + h);
            p->SendMessage(WM_AWT_RESHAPE_COMPONENT, 0, (LPARAM)r);
            // r is deleted in message handler

	    // After the input method window shown, the dimension & position may not
	    // be valid until this method is called. So we need to adjust the 
	    // IME candidate window position for the same reason as commented on
	    // awt_Frame.cpp Show() method.
	    if (p->isInputMethodWindow() && ::IsWindowVisible(p->GetHWnd())) {
	      p->AdjustCandidateWindowPos();
	    }
        }
        else
        {
            JNU_ThrowNullPointerException(env, "null target");
        }	    
    }
ret:
   env->DeleteGlobalRef(self);

   delete rfs;
}

/*
 * This is AwtWindow-specific function that is not intended for reusing
 */
HICON CreateIconFromRaster(JNIEnv* env, jintArray iconRaster, jint w, jint h) 
{
    HBITMAP mask = NULL;
    HBITMAP image = NULL;
    HICON icon = NULL;
    if (iconRaster != NULL) {
        int* iconRasterBuffer = NULL;
        try {
            iconRasterBuffer = (int *)env->GetPrimitiveArrayCritical(iconRaster, 0);
            
            JNI_CHECK_NULL_GOTO(iconRasterBuffer, "iconRaster data", done);
            
            mask = BitmapUtil::CreateTransparencyMaskFromARGB(w, h, iconRasterBuffer);
            image = BitmapUtil::CreateV4BitmapFromARGB(w, h, iconRasterBuffer);
        } catch (...) {
            if (iconRasterBuffer != NULL) {
                env->ReleasePrimitiveArrayCritical(iconRaster, iconRasterBuffer, 0);
            }
            throw;
        }
        if (iconRasterBuffer != NULL) {
            env->ReleasePrimitiveArrayCritical(iconRaster, iconRasterBuffer, 0);
        }
    }
    if (mask && image) {
        ICONINFO icnInfo;
        memset(&icnInfo, 0, sizeof(ICONINFO));
        icnInfo.hbmMask = mask;
        icnInfo.hbmColor = image;
        icnInfo.fIcon = TRUE;
        icon = ::CreateIconIndirect(&icnInfo);
    }    
    if (image) {
        destroy_BMP(image);
    }
    if (mask) {
        destroy_BMP(mask);
    }
done:
    return icon;
}

void AwtWindow::SetIconData(JNIEnv* env, jintArray iconRaster, jint w, jint h, 
                             jintArray smallIconRaster, jint smw, jint smh)
{
    HICON hOldIcon = NULL;
    HICON hOldIconSm = NULL;
    //Destroy previous icon if it isn't inherited
    if ((m_hIcon != NULL) && !m_iconInherited) {
        hOldIcon = m_hIcon;
    }
    m_hIcon = NULL;
    if ((m_hIconSm != NULL) && !m_iconInherited) {
        hOldIconSm = m_hIconSm;
    }
    m_hIconSm = NULL;
    m_hIcon = CreateIconFromRaster(env, iconRaster, w, h);
    m_hIconSm = CreateIconFromRaster(env, smallIconRaster, smw, smh);

    m_iconInherited = (m_hIcon == NULL);
    if (m_iconInherited) {
        HWND hOwner = ::GetWindow(GetHWnd(), GW_OWNER);
        AwtWindow* owner = (AwtWindow *)AwtComponent::GetComponent(hOwner);        
        if (owner != NULL) {
            m_hIcon = owner->GetHIcon();
            m_hIconSm = owner->GetHIconSm();
        } else {
            m_iconInherited = FALSE;
        }
    } 
    DoUpdateIcon();
    EnumThreadWindows(AwtToolkit::MainThread(), UpdateOwnedIconCallback, (LPARAM)this);
    if (hOldIcon != NULL) {
        DestroyIcon(hOldIcon);
    }
    if (hOldIconSm != NULL) {
        DestroyIcon(hOldIconSm);
    }
}

BOOL AwtWindow::UpdateOwnedIconCallback(HWND hWndOwned, LPARAM lParam)
{
    HWND hWndOwner = ::GetWindow(hWndOwned, GW_OWNER);
    AwtWindow* owner = (AwtWindow*)lParam;
    if (hWndOwner == owner->GetHWnd()) {
        AwtComponent* comp = AwtComponent::GetComponent(hWndOwned);
        if (comp != NULL && comp->IsTopLevel()) {
            AwtWindow* owned = (AwtWindow *)comp;
            if (owned->m_iconInherited) {
                owned->m_hIcon = owner->m_hIcon;
                owned->m_hIconSm = owner->m_hIconSm;
                owned->DoUpdateIcon();
                EnumThreadWindows(AwtToolkit::MainThread(), UpdateOwnedIconCallback, (LPARAM)owned);
            }
        }
    }
    return TRUE;
}

void AwtWindow::DoUpdateIcon()
{
    //Does nothing for windows, is overriden for frames and dialogs
}

void AwtWindow::_SetIconImagesData(void * param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SetIconImagesDataStruct* s = (SetIconImagesDataStruct*)param;
    jobject self = s->window;

    jintArray iconRaster = s->iconRaster;
    jintArray smallIconRaster = s->smallIconRaster;

    AwtWindow *window = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    // ok to pass null raster: default AWT icon

    window = (AwtWindow*)pData;
    if (::IsWindow(window->GetHWnd()))
    {
        window->SetIconData(env, iconRaster, s->w, s->h, smallIconRaster, s->smw, s->smh);

    }

ret:
    env->DeleteGlobalRef(self);
    env->DeleteGlobalRef(iconRaster);
    env->DeleteGlobalRef(smallIconRaster);
    delete s;
}

void AwtWindow::_SetMinSize(void* param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SizeStruct *ss = (SizeStruct *)param;
    jobject self = ss->window;
    jint w = ss->w;
    jint h = ss->h;
    //Perform size setting 
    AwtWindow *window = NULL;
    
    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    window = (AwtWindow *)pData;
    window->m_minSize.x = w;
    window->m_minSize.y = h;
  ret:
    env->DeleteGlobalRef(self);
    delete ss;
}

jint AwtWindow::_GetScreenImOn(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject self = (jobject)param;

    // It's entirely possible that our native resources have been destroyed 
    // before our java peer - if we're dispose()d, for instance. 
    // Alert caller w/ IllegalComponentStateException. 
    if (self == NULL) { 
        JNU_ThrowByName(env, "java/awt/IllegalComponentStateException", 
                        "Peer null in JNI"); 
        return 0; 
    } 
    PDATA pData = JNI_GET_PDATA(self); 
    if (pData == NULL) { 
        JNU_ThrowByName(env, "java/awt/IllegalComponentStateException", 
                        "Native resources unavailable"); 
        env->DeleteGlobalRef(self);
        return 0; 
    } 

    jint result = 0;
    AwtWindow *w = (AwtWindow *)pData;
    if (::IsWindow(w->GetHWnd()))
    {
        result = (jint)w->GetScreenImOn();
    }

    env->DeleteGlobalRef(self);

    return result;
}

void AwtWindow::_SetFocusableWindow(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2); 
    
    SetFocusableWindowStruct *sfws = (SetFocusableWindowStruct *)param;
    jobject self = sfws->window;
    jboolean isFocusableWindow = sfws->isFocusableWindow;
    
    AwtWindow *window = NULL;
    
    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    window = (AwtWindow *)pData;

    window->m_isFocusableWindow = isFocusableWindow;

    if (IS_WIN2000) {
        if (!window->m_isFocusableWindow) {
            LONG isPopup = window->GetStyle() & WS_POPUP;
            window->SetStyleEx(window->GetStyleEx() | (isPopup ? 0 : WS_EX_APPWINDOW) | AWT_WS_EX_NOACTIVATE);
        } else {
            window->SetStyleEx(window->GetStyleEx() & ~WS_EX_APPWINDOW & ~AWT_WS_EX_NOACTIVATE); 
        }
    } 

  ret:
    env->DeleteGlobalRef(self);
    delete sfws;
}

void AwtWindow::_ModalDisable(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2); 
    
    ModalDisableStruct *mds = (ModalDisableStruct *)param;
    jobject self = mds->window;
    HWND blockerHWnd = (HWND)mds->blockerHWnd;
    
    AwtWindow *window = NULL;
    HWND windowHWnd = 0;
    
    JNI_CHECK_NULL_GOTO(self, "peer", ret);
    PDATA pData = JNI_GET_PDATA(self);
    if (pData == NULL) {
        env->DeleteGlobalRef(self);
        delete mds;
        return;
    }

    window = (AwtWindow *)pData;
    windowHWnd = window->GetHWnd();

    if (::IsWindow(windowHWnd)) { 
        AwtWindow::SetAndActivateModalBlocker(windowHWnd, blockerHWnd); 
    } 

ret:
    env->DeleteGlobalRef(self);

    delete mds;
}

void AwtWindow::_ModalEnable(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2); 
    
    jobject self = (jobject)param;
    
    AwtWindow *window = NULL;
    HWND windowHWnd = 0;
    
    JNI_CHECK_NULL_GOTO(self, "peer", ret);
    PDATA pData = JNI_GET_PDATA(self);
    if (pData == NULL) {
        env->DeleteGlobalRef(self);
        return;
    }

    window = (AwtWindow *)pData;
    windowHWnd = window->GetHWnd();
    if (::IsWindow(windowHWnd)) {
        AwtWindow::SetModalBlocker(windowHWnd, NULL); 
    }
 
  ret:
    env->DeleteGlobalRef(self);
}

/*
 * Fixed 6353381: it's improved fix for 4792958
 * which was backed-out to avoid 5059656
 */
BOOL AwtWindow::HasValidRect()
{
    RECT inside;
    RECT outside;

    if (::IsIconic(GetHWnd())) {
        return FALSE;
    }

    ::GetClientRect(GetHWnd(), &inside);
    ::GetWindowRect(GetHWnd(), &outside);

    BOOL isZeroClientArea = (inside.right == 0 && inside.bottom == 0);
    BOOL isInvalidLocation = ((outside.left == -32000 && outside.top == -32000) || // Win2k && WinXP
                              (outside.left == 32000 && outside.top == 32000) || // Win95 && Win98
                              (outside.left == 3000 && outside.top == 3000)); // Win95 && Win98

    // the bounds correspond to iconic state
    if (isZeroClientArea && isInvalidLocation)
    {
        return FALSE;
    }

    return TRUE;
}

//XXX: If we ever had a chance to use _WIN32_WINNT >= 0x0500, 
//     we could eliminate these definitions.
#if (_WIN32_WINNT < 0x0500)

#define LWA_COLORKEY       (0x00000001)

#define ULW_COLORKEY       (0x00000001)
#define ULW_ALPHA          (0x00000002)
#define ULW_OPAQUE         (0x00000004)

#endif // (_WIN32_WINNT < 0x0500)

void AwtWindow::_SetOpacity(void* param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    OpacityStruct *os = (OpacityStruct *)param;
    jobject self = os->window;
    BYTE iOpacity = (BYTE)os->iOpacity;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    AwtWindow *window = (AwtWindow *)pData;

    window->SetTranslucency(iOpacity, window->isOpaque());

  ret:
    env->DeleteGlobalRef(self);
    delete os;
}

void AwtWindow::_SetOpaque(void* param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    OpaqueStruct *os = (OpaqueStruct *)param;
    jobject self = os->window;
    BOOL isOpaque = (BOOL)os->isOpaque;
    
    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    AwtWindow *window = (AwtWindow *)pData;

    window->SetTranslucency(window->getOpacity(), isOpaque);

  ret:
    env->DeleteGlobalRef(self);
    delete os;
}

void AwtWindow::RedrawWindow()
{
    if (isOpaque()) {
        ::RedrawWindow(GetHWnd(), NULL, NULL,
                RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    } else {
        ::EnterCriticalSection(&contentBitmapCS);
        if (hContentBitmap != NULL) {
            UpdateWindowImpl(contentWidth, contentHeight, hContentBitmap);
        }
        ::LeaveCriticalSection(&contentBitmapCS);
    }
}

void AwtWindow::SetTranslucency(BYTE opacity, BOOL opaque)
{
    BYTE old_opacity = getOpacity();
    BOOL old_opaque = isOpaque();

    if (opacity == old_opacity && opaque == old_opaque) {
        return;
    }

    load_user_procs();
    if (!fn_set_layered_window_attributes || !fn_update_layered_window) {
        // The OS does not support the effects.
        return;
    }
    
    setOpacity(opacity);
    setOpaque(opaque);

    HWND hwnd = GetHWnd();

    if (opaque != old_opaque) {
        ::EnterCriticalSection(&contentBitmapCS);
        if (hContentBitmap != NULL) {
            ::DeleteObject(hContentBitmap);
            hContentBitmap = NULL;
        }
        ::LeaveCriticalSection(&contentBitmapCS);
    }

    if (opaque && opacity == 0xff) {
        // Turn off all the effects
        AwtWindow::SetLayered(hwnd, false);

        // Ask the window to repaint itself and all the children
        RedrawWindow();
    } else {
        // We're going to enable some effects
        if (!AwtWindow::IsLayered(hwnd)) {
            AwtWindow::SetLayered(hwnd, true);
        } else {
            if ((opaque && opacity < 0xff) ^ (old_opaque && old_opacity < 0xff)) {
                // _One_ of the modes uses the SetLayeredWindowAttributes. 
                // Need to reset the style in this case.
                // If both modes are simple (i.e. just changing the opacity level),
                // no need to reset the style.
                AwtWindow::SetLayered(hwnd, false);
                AwtWindow::SetLayered(hwnd, true);
            }
        }

        if (opaque) {
            // Simple opacity mode
            (*fn_set_layered_window_attributes)(hwnd, RGB(0, 0, 0), 
                    opacity, AWT_LWA_ALPHA);
        }
    }
}

void AwtWindow::_UpdateWindow(void* param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    UpdateWindowStruct *uws = (UpdateWindowStruct *)param;
    jobject self = uws->window;
    jintArray data = uws->data;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    AwtWindow *window = (AwtWindow *)pData;

    window->UpdateWindow(env, data, (int)uws->width, (int)uws->height,
                         uws->hBitmap);

  ret:
    env->DeleteGlobalRef(self);
    if (data != NULL) {
        env->DeleteGlobalRef(data);
    }
    delete uws;
}

static HBITMAP CreateBitmapFromRaster(JNIEnv* env, jintArray raster, jint w, jint h)
{
    HBITMAP image = NULL;
    if (raster != NULL) {
        int* rasterBuffer = NULL;
        try {
            rasterBuffer = (int *)env->GetPrimitiveArrayCritical(raster, 0);

            JNI_CHECK_NULL_GOTO(rasterBuffer, "raster data", done);

            image = BitmapUtil::CreateBitmapFromARGBPre(w, h, w*4, rasterBuffer);
        } catch (...) {
            if (rasterBuffer != NULL) {
                env->ReleasePrimitiveArrayCritical(raster, rasterBuffer, 0);
            }
            throw;
        }
        if (rasterBuffer != NULL) {
            env->ReleasePrimitiveArrayCritical(raster, rasterBuffer, 0);
        }
    }
done:
    return image;
}

void AwtWindow::UpdateWindowImpl(int width, int height, HBITMAP hBitmap)
{
    if (isOpaque()) {
        return;
    }

    load_user_procs();
    if (!fn_update_layered_window) {
        // The OS does not support the effects.
        return;
    }

    HWND hWnd = GetHWnd();
    HDC hdcDst = ::GetDC(NULL);
    HDC hdcSrc = ::CreateCompatibleDC(NULL);
    HBITMAP hOldBitmap = (HBITMAP)::SelectObject(hdcSrc, hBitmap);

    //XXX: this code doesn't paint the children (say, the java.awt.Button)!
    //So, if we ever want to support HWs here, we need to repaint them 
    //in some other way...
    //::SendMessage(hWnd, WM_PRINT, (WPARAM)hdcSrc, /*PRF_CHECKVISIBLE |*/ 
    //      PRF_CHILDREN /*| PRF_CLIENT | PRF_NONCLIENT*/);

    POINT ptSrc;
    ptSrc.x = ptSrc.y = 0;

    RECT rect;
    POINT ptDst;
    SIZE size;

    ::GetWindowRect(hWnd, &rect);
    ptDst.x = rect.left;
    ptDst.y = rect.top;
    size.cx = width;
    size.cy = height;

    BLENDFUNCTION bf;

    bf.SourceConstantAlpha = getOpacity();
    bf.AlphaFormat = AC_SRC_ALPHA;
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    
    (*fn_update_layered_window)(hWnd, hdcDst, &ptDst, &size, hdcSrc, &ptSrc, 
            RGB(0, 0, 0), &bf, ULW_ALPHA);

    ::ReleaseDC(NULL, hdcDst);
    ::SelectObject(hdcSrc, hOldBitmap);
    ::DeleteDC(hdcSrc);
}

void AwtWindow::UpdateWindow(JNIEnv* env, jintArray data, int width, int height,
                             HBITMAP hNewBitmap)
{
    if (isOpaque()) {
        return;
    }

    load_user_procs();
    if (!fn_update_layered_window) {
        // The OS does not support the effects.
        return;
    }

    HBITMAP hBitmap;
    if (hNewBitmap == NULL) {
        if (data == NULL) {
            return;
        }
        hBitmap = CreateBitmapFromRaster(env, data, width, height);
        if (hBitmap == NULL) {
            return;
        }
    } else {
        hBitmap = hNewBitmap;
    }

    ::EnterCriticalSection(&contentBitmapCS);
    if (hContentBitmap != NULL) {
        ::DeleteObject(hContentBitmap);
    }
    hContentBitmap = hBitmap;
    contentWidth = width;
    contentHeight = height;
    UpdateWindowImpl(width, height, hBitmap);
    ::LeaveCriticalSection(&contentBitmapCS);
}

extern "C" {

/*
 * Class:     java_awt_Window
 * Method:    initIDs
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_java_awt_Window_initIDs(JNIEnv *env, jclass cls)
{
    TRY;

    AwtWindow::warningStringID =
        env->GetFieldID(cls, "warningString", "Ljava/lang/String;");
    AwtWindow::locationByPlatformID =
        env->GetFieldID(cls, "locationByPlatform", "Z");
    AwtWindow::securityWarningWidthID =
        env->GetFieldID(cls, "securityWarningWidth", "I");
    AwtWindow::securityWarningHeightID =
        env->GetFieldID(cls, "securityWarningHeight", "I");
    AwtWindow::getWarningStringMID =
        env->GetMethodID(cls, "getWarningString", "()Ljava/lang/String;");
    AwtWindow::calculateSecurityWarningPositionMID =
        env->GetMethodID(cls, "calculateSecurityWarningPosition", "(DDDD)Ljava/awt/geom/Point2D;");

    CATCH_BAD_ALLOC;
}

} /* extern "C" */


/************************************************************************
 * WindowPeer native methods
 */

extern "C" {

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    initIDs
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_initIDs(JNIEnv *env, jclass cls)
{
    TRY;

    AwtWindow::sysXID = env->GetFieldID(cls, "sysX", "I");
    AwtWindow::sysYID = env->GetFieldID(cls, "sysY", "I");
    AwtWindow::sysWID = env->GetFieldID(cls, "sysW", "I");
    AwtWindow::sysHID = env->GetFieldID(cls, "sysH", "I");

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    toFront
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer__1toFront(JNIEnv *env, jobject self)
{
    TRY;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_ToFront,
        env->NewGlobalRef(self));
    // global ref is deleted in _ToFront()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    toBack
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_toBack(JNIEnv *env, jobject self)
{
    TRY;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_ToBack,
        env->NewGlobalRef(self));
    // global ref is deleted in _ToBack()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    setAlwaysOnTop
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_setAlwaysOnTopNative(JNIEnv *env, jobject self,
                                                jboolean value)
{
    TRY;
  
    SetAlwaysOnTopStruct *sas = new SetAlwaysOnTopStruct;
    sas->window = env->NewGlobalRef(self);
    sas->value = value;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_SetAlwaysOnTop, sas);
    // global ref and sas are deleted in _SetAlwaysOnTop
  
    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    _setTitle
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer__1setTitle(JNIEnv *env, jobject self,
                                            jstring title)
{
    TRY;

    SetTitleStruct *sts = new SetTitleStruct;
    sts->window = env->NewGlobalRef(self);
    sts->title = (jstring)env->NewGlobalRef(title);

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_SetTitle, sts);
    /// global refs and sts are deleted in _SetTitle()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    _setResizable
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer__1setResizable(JNIEnv *env, jobject self,
                                                jboolean resizable)
{
    TRY;

    SetResizableStruct *srs = new SetResizableStruct;
    srs->window = env->NewGlobalRef(self);
    srs->resizable = resizable;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_SetResizable, srs);
    // global ref and srs are deleted in _SetResizable

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    create
 * Signature: (Lsun/awt/windows/WComponentPeer;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_createAwtWindow(JNIEnv *env, jobject self, 
                                                 jobject parent)
{
    TRY;

    PDATA pData;
//    JNI_CHECK_PEER_RETURN(parent);
    AwtToolkit::CreateComponent(self, parent,
                                (AwtToolkit::ComponentFactory)
                                AwtWindow::Create);
    JNI_CHECK_PEER_CREATION_RETURN(self);

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    updateInsets
 * Signature: (Ljava/awt/Insets;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_updateInsets(JNIEnv *env, jobject self,
                                              jobject insets)
{
    TRY;

    UpdateInsetsStruct *uis = new UpdateInsetsStruct;
    uis->window = env->NewGlobalRef(self);
    uis->insets = env->NewGlobalRef(insets);

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_UpdateInsets, uis);
    // global refs and uis are deleted in _UpdateInsets()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    reshapeFrame
 * Signature: (IIII)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_reshapeFrame(JNIEnv *env, jobject self,
                                        jint x, jint y, jint w, jint h) 
{
    TRY;

    ReshapeFrameStruct *rfs = new ReshapeFrameStruct;
    rfs->frame = env->NewGlobalRef(self);
    rfs->x = x;
    rfs->y = y;
    rfs->w = w;
    rfs->h = h;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_ReshapeFrame, rfs);
    // global ref and rfs are deleted in _ReshapeFrame()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    getSysMinWidth
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WWindowPeer_getSysMinWidth(JNIEnv *env, jclass self)
{
    TRY;

    return ::GetSystemMetrics(SM_CXMIN);

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    getSysMinHeight
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WWindowPeer_getSysMinHeight(JNIEnv *env, jclass self)
{
    TRY;

    return ::GetSystemMetrics(SM_CYMIN);

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    getSysIconHeight
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WWindowPeer_getSysIconHeight(JNIEnv *env, jclass self)
{
    TRY;

    return ::GetSystemMetrics(SM_CYICON);

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    getSysIconWidth
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WWindowPeer_getSysIconWidth(JNIEnv *env, jclass self)
{
    TRY;

    return ::GetSystemMetrics(SM_CXICON);

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    getSysSmIconHeight
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WWindowPeer_getSysSmIconHeight(JNIEnv *env, jclass self)
{
    TRY;

    return ::GetSystemMetrics(SM_CYSMICON);

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    getSysSmIconWidth
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WWindowPeer_getSysSmIconWidth(JNIEnv *env, jclass self)
{
    TRY;

    return ::GetSystemMetrics(SM_CXSMICON);

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    setIconImagesData
 * Signature: ([I)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_setIconImagesData(JNIEnv *env, jobject self, 
    jintArray iconRaster, jint w, jint h,
    jintArray smallIconRaster, jint smw, jint smh)
{
    TRY;

    SetIconImagesDataStruct *sims = new SetIconImagesDataStruct;

    sims->window = env->NewGlobalRef(self);
    sims->iconRaster = (jintArray)env->NewGlobalRef(iconRaster);
    sims->w = w;
    sims->h = h;
    sims->smallIconRaster = (jintArray)env->NewGlobalRef(smallIconRaster);
    sims->smw = smw;
    sims->smh = smh;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_SetIconImagesData, sims);
    // global refs and sims are deleted in _SetIconImagesData()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    setMinSize
 * Signature: (Lsun/awt/windows/WWindowPeer;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_setMinSize(JNIEnv *env, jobject self,
                                              jint w, jint h)
{
    TRY;

    SizeStruct *ss = new SizeStruct;
    ss->window = env->NewGlobalRef(self);
    ss->w = w;
    ss->h = h;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_SetMinSize, ss);
    // global refs and mds are deleted in _SetMinSize

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    getScreenImOn
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WWindowPeer_getScreenImOn(JNIEnv *env, jobject self)
{
    TRY;

    return static_cast<jint>(reinterpret_cast<INT_PTR>(AwtToolkit::GetInstance().SyncCall(
        (void *(*)(void *))AwtWindow::_GetScreenImOn,
        env->NewGlobalRef(self))));
    // global ref is deleted in _GetScreenImOn()

    CATCH_BAD_ALLOC_RET(-1);
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    modalDisable
 * Signature: (J)V 
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_modalDisable(JNIEnv *env, jobject self,
                                              jobject blocker, jlong blockerHWnd) 
{
    TRY;

    ModalDisableStruct *mds = new ModalDisableStruct;
    mds->window = env->NewGlobalRef(self);
    mds->blockerHWnd = blockerHWnd;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_ModalDisable, mds);
    // global ref and mds are deleted in _ModalDisable

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    modalEnable
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_modalEnable(JNIEnv *env, jobject self, jobject blocker) 
{
    TRY;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_ModalEnable,
        env->NewGlobalRef(self));
    // global ref is deleted in _ModalEnable

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    setFocusableWindow 
 * Signature: (Z)V 
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_setFocusableWindow(JNIEnv *env, jobject self, jboolean isFocusableWindow) 
{
    TRY;
    
    SetFocusableWindowStruct *sfws = new SetFocusableWindowStruct;
    sfws->window = env->NewGlobalRef(self);
    sfws->isFocusableWindow = isFocusableWindow;
    
    AwtToolkit::GetInstance().SyncCall(AwtWindow::_SetFocusableWindow, sfws);
    // global ref and sfws are deleted in _SetFocusableWindow()

    CATCH_BAD_ALLOC;
}

JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_nativeGrab(JNIEnv *env, jobject self) 
{
    TRY;
    
    AwtToolkit::GetInstance().SyncCall(AwtWindow::_Grab, env->NewGlobalRef(self));
    // global ref is deleted in _Grab()

    CATCH_BAD_ALLOC;
}

JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_nativeUngrab(JNIEnv *env, jobject self) 
{
    TRY;
    
    AwtToolkit::GetInstance().SyncCall(AwtWindow::_Ungrab, env->NewGlobalRef(self));
    // global ref is deleted in _Ungrab()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    setOpacity
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_setOpacity(JNIEnv *env, jobject self,
                                              jint iOpacity)
{
    TRY;

    OpacityStruct *os = new OpacityStruct;
    os->window = env->NewGlobalRef(self);
    os->iOpacity = iOpacity;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_SetOpacity, os);
    // global refs and mds are deleted in _SetMinSize

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    setOpaqueImpl
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_setOpaqueImpl(JNIEnv *env, jobject self,
                                              jboolean isOpaque)
{
    TRY;

    OpaqueStruct *os = new OpaqueStruct;
    os->window = env->NewGlobalRef(self);
    os->isOpaque = isOpaque;

    AwtToolkit::GetInstance().SyncCall(AwtWindow::_SetOpaque, os);
    // global refs and mds are deleted in _SetMinSize

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    updateWindowImpl
 * Signature: ([III)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_updateWindowImpl(JNIEnv *env, jobject self,
                                                  jintArray data,
                                                  jint width, jint height)
{
    TRY;

    UpdateWindowStruct *uws = new UpdateWindowStruct;
    uws->window = env->NewGlobalRef(self);
    uws->data = (jintArray)env->NewGlobalRef(data);
    uws->hBitmap = NULL;
    uws->width = width;
    uws->height = height;

    AwtToolkit::GetInstance().InvokeFunction(AwtWindow::_UpdateWindow, uws);
    // global refs and mds are deleted in _UpdateWindow

    CATCH_BAD_ALLOC;
}

/**
 * This method is called from the WGL pipeline when it needs to update
 * the layered window WindowPeer's C++ level object.
 */
void AwtWindow_UpdateWindow(JNIEnv *env, jobject peer,
                            jint width, jint height, HBITMAP hBitmap)
{
    TRY;

    UpdateWindowStruct *uws = new UpdateWindowStruct;
    uws->window = env->NewGlobalRef(peer);
    uws->data = NULL;
    uws->hBitmap = hBitmap;
    uws->width = width;
    uws->height = height;

    AwtToolkit::GetInstance().InvokeFunction(AwtWindow::_UpdateWindow, uws);
    // global refs and mds are deleted in _UpdateWindow

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WWindowPeer
 * Method:    repositionSecurityWarning
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WWindowPeer_repositionSecurityWarning(JNIEnv *env,
        jobject self)
{
    TRY;

    RepositionSecurityWarningStruct *rsws =
        new RepositionSecurityWarningStruct;
    rsws->window = env->NewGlobalRef(self);

    AwtToolkit::GetInstance().InvokeFunction(
            AwtWindow::_RepositionSecurityWarning, rsws);
    // global refs and mds are deleted in _RepositionSecurityWarning

    CATCH_BAD_ALLOC;
}

} /* extern "C" */
