/*
 * @(#)awt_Frame.cpp	1.170 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include "awt_Toolkit.h"
#include "awt_Frame.h"
#include "awt_MenuBar.h"
#include "awt_Dialog.h"
#include "awt_IconCursor.h"
#include "awt_Win32GraphicsDevice.h"
#include "ComCtl32Util.h"

#include <windowsx.h>

#include <java_lang_Integer.h>
#include <sun_awt_EmbeddedFrame.h>
#include <sun_awt_windows_WEmbeddedFrame.h>
#include <sun_awt_windows_WEmbeddedFramePeer.h>


BOOL isAppActive = FALSE;

/* IMPORTANT! Read the README.JNI file for notes on JNI converted AWT code.
 */

/***********************************************************************/
// Struct for _SetState() method
struct SetStateStruct {
    jobject frame;
    jint state;
};
// Struct for _SetMaximizedBounds() method
struct SetMaximizedBoundsStruct {
    jobject frame;
    jint x, y;
    jint width, height;
};
// Struct for _SetMenuBar() method
struct SetMenuBarStruct {
    jobject frame;
    jobject menubar;
};

// Struct for _SetIMMOption() method
struct SetIMMOptionStruct {
    jobject frame;
    jstring option;
};
// Struct for _SynthesizeWmActivate() method
struct SynthesizeWmActivateStruct {
    jobject frame;
    jboolean doActivate;
};
// Struct for _NotifyModalBlocked() method
struct NotifyModalBlockedStruct {
    jobject frame;
    jobject peer;
    jobject blockerPeer;
    jboolean blocked;
};
// Information about thread containing modal blocked embedded frames
struct BlockedThreadStruct {
    int framesCount;
    HHOOK mouseHook;
    HHOOK modalHook;
};
/************************************************************************
 * AwtFrame fields
 */

jfieldID AwtFrame::handleID;
jfieldID AwtFrame::stateID;
jfieldID AwtFrame::undecoratedID;

jmethodID AwtFrame::activateEmbeddingTopLevelMID;

Hashtable AwtFrame::sm_BlockedThreads("AWTBlockedThreads");

/************************************************************************
 * AwtFrame methods
 */

AwtFrame::AwtFrame() {
    m_parentWnd = NULL;
    menuBar = NULL;
    m_isEmbedded = FALSE;
    m_ignoreWmSize = FALSE;
    m_isMenuDropped = FALSE;
    m_isInputMethodWindow = FALSE;
    m_isUndecorated = FALSE;
    m_proxyFocusOwner = NULL;
    m_lastProxiedFocusOwner = NULL;
    m_actualFocusedWindow = NULL;
    m_iconic = FALSE;
    m_zoomed = FALSE;
    m_maxBoundsSet = FALSE;
    m_isEmbeddedFrameActivationRequest = FALSE;

    isInManualMoveOrSize = FALSE;
    grabbedHitTest = 0;
}

AwtFrame::~AwtFrame() {
    DestroyProxyFocusOwner();
}

LPCTSTR AwtFrame::GetClassName() {
    return AWT_FRAME_WINDOW_CLASS_NAME;
}

/*
 * Create a new AwtFrame object and window.
 */
AwtFrame* AwtFrame::Create(jobject self, jobject parent)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    if (env->EnsureLocalCapacity(1) < 0) {
	return NULL;
    }

    PDATA pData;
    HWND hwndParent = NULL;
    AwtFrame* frame;
    jclass cls = NULL;
    jclass inputMethodWindowCls = NULL;
    jobject target = NULL;

    try {
        target = env->GetObjectField(self, AwtObject::targetID);
	JNI_CHECK_NULL_GOTO(target, "target", done);

	if (parent != NULL) {
	    JNI_CHECK_PEER_GOTO(parent, done);
	    {
	        AwtFrame* parent = (AwtFrame *)pData;
		hwndParent = parent->GetHWnd();
	    }
	}

	frame = new AwtFrame();
	
	{
	    /*
	     * A variation on Netscape's hack for embedded frames: the client
	     * area of the browser is a Java Frame for parenting purposes, but
	     * really a Windows child window
	     */
	    cls = env->FindClass("sun/awt/EmbeddedFrame");
	    if (cls == NULL) {
	        return NULL;
	    }
	    INT_PTR handle;
	    jboolean isEmbeddedInstance = env->IsInstanceOf(target, cls);
	    jboolean isEmbedded = FALSE;

	    if (isEmbeddedInstance) {
                handle = static_cast<INT_PTR>(env->GetLongField(target, AwtFrame::handleID));
		if (handle != 0) {
		    isEmbedded = TRUE;
		}
	    }
	    frame->m_isEmbedded = isEmbedded;
	    
	    if (isEmbedded) {
                hwndParent = (HWND)handle;
                RECT rect;
                ::GetClientRect(hwndParent, &rect);
                //Fix for 6328675: SWT_AWT.new_Frame doesn't occupy client area under JDK6
                frame->m_isUndecorated = true;
                /* 
                 * Fix for BugTraq ID 4337754.
                 * Initialize m_peerObject before the first call 
                 * to AwtFrame::GetClassName().
                 */
                frame->m_peerObject = env->NewGlobalRef(self);
                frame->RegisterClass();
                DWORD exStyle = WS_EX_NOPARENTNOTIFY;

                if (GetRTL()) {
                    exStyle |= WS_EX_RIGHT | WS_EX_LEFTSCROLLBAR;
                    if (GetRTLReadingOrder())
                        exStyle |= WS_EX_RTLREADING;
                }

                frame->m_hwnd = ::CreateWindowEx(exStyle,
                                                 frame->GetClassName(), TEXT(""),
                                                 WS_CHILD | WS_CLIPCHILDREN,
                                                 0, 0,
                                                 rect.right, rect.bottom,
                                                 hwndParent, NULL,
                                                 AwtToolkit::GetInstance().GetModuleHandle(),
                                                 NULL);
                frame->LinkObjects(env, self);
                frame->SubclassHWND();

                // Update target's dimensions to reflect this embedded window.
                ::GetClientRect(frame->m_hwnd, &rect);
                ::MapWindowPoints(frame->m_hwnd, hwndParent, (LPPOINT)&rect,
                                  2);                

                env->SetIntField(target, AwtComponent::xID, rect.left);
                env->SetIntField(target, AwtComponent::yID, rect.top);
                env->SetIntField(target, AwtComponent::widthID,
                                 rect.right-rect.left);
                env->SetIntField(target, AwtComponent::heightID,
                                 rect.bottom-rect.top);
                frame->InitPeerGraphicsConfig(env, self);
	    } else {
	        jint state = env->GetIntField(target, AwtFrame::stateID);
		DWORD exStyle;
		DWORD style;

               // for input method windows, use minimal decorations
               inputMethodWindowCls = env->FindClass("sun/awt/im/InputMethodWindow");
               if ((inputMethodWindowCls != NULL) && env->IsInstanceOf(target, inputMethodWindowCls)) {
                   //for below-the-spot composition window, use no decoration
                   if (env->GetBooleanField(target, AwtFrame::undecoratedID) == JNI_TRUE){
                        exStyle = 0;
                        style = WS_POPUP|WS_CLIPCHILDREN;
                        frame->m_isUndecorated = TRUE; 
                   } else {
                        exStyle = WS_EX_PALETTEWINDOW;
                        style = WS_CLIPCHILDREN;
                   }
                   frame->m_isInputMethodWindow = TRUE;
                } else if (env->GetBooleanField(target, AwtFrame::undecoratedID) == JNI_TRUE) {
                    exStyle = 0;
                    style = WS_POPUP | WS_SYSMENU | WS_CLIPCHILDREN |
                        WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
                  if (state & java_awt_Frame_ICONIFIED) {
                      style |= WS_ICONIC;
                      frame->setIconic(TRUE);
                  }
                    frame->m_isUndecorated = TRUE;
		} else {
		    exStyle = WS_EX_WINDOWEDGE;
		    style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
                  if (state & java_awt_Frame_ICONIFIED) {
                      style |= WS_ICONIC;
                      frame->setIconic(TRUE);
                  }
		}

		if (GetRTL()) {
		    exStyle |= WS_EX_RIGHT | WS_EX_LEFTSCROLLBAR;
		    if (GetRTLReadingOrder())
		        exStyle |= WS_EX_RTLREADING;
		}

                jint x = env->GetIntField(target, AwtComponent::xID);
                jint y = env->GetIntField(target, AwtComponent::yID);
                jint width = env->GetIntField(target, AwtComponent::widthID);
                jint height = env->GetIntField(target, AwtComponent::heightID);

		frame->CreateHWnd(env, L"",
		                  style,
				  exStyle,
				  0, 0, 0, 0,
				  hwndParent,
				  NULL,
				  ::GetSysColor(COLOR_WINDOWTEXT),
				  ::GetSysColor(COLOR_WINDOWFRAME),
				  self);

		/* 
		 * Reshape here instead of during create, so that a
		 * WM_NCCALCSIZE is sent. 
		 */
		frame->Reshape(x, y, width, height);
	    }
	}
    } catch (...) {
        env->DeleteLocalRef(target);
	env->DeleteLocalRef(cls);
	env->DeleteLocalRef(inputMethodWindowCls);
	throw;
    }

done:
    env->DeleteLocalRef(target);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(inputMethodWindowCls);

    return frame;
}

LRESULT CALLBACK AwtFrame::ProxyWindowProc(HWND hwnd, UINT message,
					   WPARAM wParam, LPARAM lParam)
{
    TRY;

    DASSERT(::IsWindow(hwnd));

    AwtFrame *parent = (AwtFrame *)
        AwtComponent::GetComponentImpl(::GetParent(hwnd));

    if (!parent || parent->GetProxyFocusOwner() != hwnd ||
            message == AwtComponent::WmAwtIsComponent ||
            message == WM_GETOBJECT)
    {
        return ComCtl32Util::GetInstance().DefWindowProc(NULL, hwnd, message, wParam, lParam);
    }

    AwtComponent *p = NULL;
    // IME and input language related messages need to be sent to a window 
    // which has the Java input focus
    switch (message) {
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_IME_COMPOSITION:
        case WM_IME_SETCONTEXT:
        case WM_IME_NOTIFY:
        case WM_IME_CONTROL:
        case WM_IME_COMPOSITIONFULL:
        case WM_IME_SELECT:
        case WM_IME_CHAR:
        case 0x0288: //WM_IME_REQUEST
        case WM_IME_KEYDOWN:
        case WM_IME_KEYUP:
        case WM_INPUTLANGCHANGEREQUEST:
        case WM_INPUTLANGCHANGE:
            p = AwtComponent::GetComponent(parent->GetLastProxiedFocusOwner());
            if  (p!= NULL) {
                return p->WindowProc(message, wParam, lParam);
            }
            break;
        case 0x0127: // WM_CHANGEUISTATE
        case 0x0128: // WM_UPDATEUISTATE
            return 0;
    }

    return parent->WindowProc(message, wParam, lParam);

    CATCH_BAD_ALLOC_RET(0);
}

void AwtFrame::CreateProxyFocusOwner()
{
    DASSERT(m_proxyFocusOwner == NULL);
    DASSERT(AwtToolkit::MainThread() == ::GetCurrentThreadId());

    m_proxyFocusOwner = ::CreateWindow(TEXT("STATIC"), 
                                       TEXT("ProxyFocusOwner"), 
                                       WS_CHILD,
				       0, 0, 0, 0, GetHWnd(), NULL,
				       AwtToolkit::GetInstance().
				           GetModuleHandle(),
				       NULL);

    m_proxyDefWindowProc = ComCtl32Util::GetInstance().SubclassHWND(m_proxyFocusOwner, ProxyWindowProc);

}

void AwtFrame::DestroyProxyFocusOwner()
{
    DASSERT(AwtToolkit::MainThread() == ::GetCurrentThreadId());

    if (m_proxyFocusOwner != NULL) {
        HWND toDestroy = m_proxyFocusOwner;
        m_proxyFocusOwner = NULL;
        ComCtl32Util::GetInstance().UnsubclassHWND(toDestroy, ProxyWindowProc, m_proxyDefWindowProc);
        ::DestroyWindow(toDestroy);
    }
}

MsgRouting AwtFrame::WmShowWindow(BOOL show, UINT status)
{
    /*
     * Fix for 6492970. In some cases the native platform doesn't set newly
     * shown frame/dialog the foreground window. Even worse, at the same time
     * it could send it WM_ACTIVATE message.
     * We set it foreground programmatically.
     * (See also: 6599270)
     */
    if (!IsEmbeddedFrame() && show == TRUE && status == 0) {
        HWND fgHWnd = ::GetForegroundWindow();
        if (fgHWnd != NULL) {
            DWORD fgProcessID;
            ::GetWindowThreadProcessId(fgHWnd, &fgProcessID);

            if (fgProcessID != ::GetCurrentProcessId()) {
                AwtWindow* window = (AwtWindow*)GetComponent(GetHWnd());

                if (window != NULL && window->IsFocusableWindow() &&
                    !::IsWindow(GetModalBlocker(GetHWnd())))
                {
                    // When the Java process is not allowed to set the foreground window
                    // (see MSDN) the request below will just have no effect.
                    ::SetForegroundWindow(GetHWnd());
                }
            }
        }
    }
    return AwtWindow::WmShowWindow(show, status);
}

MsgRouting AwtFrame::WmMouseUp(UINT flags, int x, int y, int button) {
    if (isInManualMoveOrSize) {
        isInManualMoveOrSize = FALSE;
        ::ReleaseCapture();
        return mrConsume;
    }
    return AwtWindow::WmMouseUp(flags, x, y, button);
}

MsgRouting AwtFrame::WmMouseMove(UINT flags, int x, int y) {
    /**
     * If this Frame is non-focusable then we should implement move and size operation for it by 
     * ourselfves because we don't dispatch appropriate mouse messages to default window procedure.
     */
    if (!IsFocusableWindow() && isInManualMoveOrSize) {        
        DWORD curPos = ::GetMessagePos();
        x = GET_X_LPARAM(curPos);
        y = GET_Y_LPARAM(curPos);
        RECT r;
        ::GetWindowRect(GetHWnd(), &r);
        POINT mouseLoc = {x, y};
        mouseLoc.x -= savedMousePos.x;
        mouseLoc.y -= savedMousePos.y;
        savedMousePos.x = x;
        savedMousePos.y = y;
        if (grabbedHitTest == HTCAPTION) {
            ::SetWindowPos(GetHWnd(), NULL, r.left+mouseLoc.x, r.top+mouseLoc.y, 
                           r.right-r.left, r.bottom-r.top, 
                           SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
        } else {
            switch (grabbedHitTest) {
            case HTTOP:
                r.top += mouseLoc.y;
                break;
            case HTBOTTOM: 
                r.bottom += mouseLoc.y;
                break;
            case HTRIGHT:
                r.right += mouseLoc.x;
                break;
            case HTLEFT:
                r.left += mouseLoc.x;
                break;
            case HTTOPLEFT:
                r.left += mouseLoc.x;
                r.top += mouseLoc.y;
                break;
            case HTTOPRIGHT:
                r.top += mouseLoc.y;
                r.right += mouseLoc.x;
                break;
            case HTBOTTOMLEFT:
                r.left += mouseLoc.x;
                r.bottom += mouseLoc.y;
                break;
            case HTBOTTOMRIGHT:
            case HTSIZE:
                r.right += mouseLoc.x;
                r.bottom += mouseLoc.y;
                break;                
            }
                
            ::SetWindowPos(GetHWnd(), NULL, r.left, r.top, 
                           r.right-r.left, r.bottom-r.top, 
                           SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_NOZORDER | 
                           SWP_NOCOPYBITS | SWP_DEFERERASE);
        }
        return mrConsume;
    } else {
        return AwtWindow::WmMouseMove(flags, x, y);
    }
}

MsgRouting AwtFrame::WmNcMouseDown(WPARAM hitTest, int x, int y, int button) {
    // By Swing request, click on the Frame's decorations (even on
    // grabbed Frame) should generate UngrabEvent
    if (m_grabbedWindow != NULL/* && !m_grabbedWindow->IsOneOfOwnersOf(this)*/) {
        m_grabbedWindow->Ungrab();
    }

    if (!IsFocusableWindow() && (button & LEFT_BUTTON) != LEFT_BUTTON) {        
        /**
         * If this Frame is non-focusable then we should implement move and size operation for it by 
         * ourselfves because we don't dispatch appropriate mouse messages to default window procedure.
         */
        if ((button & DBL_CLICK) == DBL_CLICK && hitTest == HTCAPTION) {
            // Double click on caption - maximize or restore Frame.
            if (IsResizable()) {
                if (::IsZoomed(GetHWnd())) {
                    ::ShowWindow(GetHWnd(), SW_SHOWNOACTIVATE);
                } else {                             
                    ::ShowWindow(GetHWnd(), SW_MAXIMIZE);
                }
            }
            return mrConsume;
        }
        switch (hitTest) {
          case HTMAXBUTTON:
              if (IsResizable()) {
                  if (::IsZoomed(GetHWnd())) {
                      ::ShowWindow(GetHWnd(), SW_SHOWNOACTIVATE);
                  } else {                             
                      ::ShowWindow(GetHWnd(), SW_MAXIMIZE);
                  }
              }            
              return mrConsume;
          case HTCAPTION: 
          case HTTOP:
          case HTBOTTOM:
          case HTLEFT:
          case HTRIGHT:
          case HTTOPLEFT:
          case HTTOPRIGHT:
          case HTBOTTOMLEFT:
          case HTBOTTOMRIGHT:
          case HTSIZE:
              // We are going to perform default mouse action on non-client area of this window
              // Grab mouse for this purpose and store coordinates for motion vector calculation
              savedMousePos.x = x;
              savedMousePos.y = y;
              ::SetCapture(GetHWnd());        
              isInManualMoveOrSize = TRUE;
              grabbedHitTest = hitTest;
              return mrConsume;
          default:
              return mrDoDefault;
        }
    } 
    return AwtWindow::WmNcMouseDown(hitTest, x, y, button);
}

/* Show the frame in it's current state */
void
AwtFrame::Show()
{
    m_visible = true;
    HWND hwnd = GetHWnd();
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    DTRACE_PRINTLN3("AwtFrame::Show:%s%s%s",
                  m_iconic ? " iconic" : "",
                  m_zoomed ? " zoomed" : "",
                  m_iconic || m_zoomed ? "" : " normal");

    BOOL locationByPlatform = env->GetBooleanField(GetTarget(env), AwtWindow::locationByPlatformID);

    if (locationByPlatform) {
         moveToDefaultLocation();
    }
	    
    if (m_iconic) {
	if (m_zoomed) {
	    // This whole function could probably be rewritten to use
	    // ::SetWindowPlacement but MS docs doesn't tell if
	    // ::SetWindowPlacement is a proper superset of
	    // ::ShowWindow.  So let's be conservative and only use it
	    // here, where we really do need it.
	    DTRACE_PRINTLN("AwtFrame::Show(SW_SHOWMINIMIZED, WPF_RESTORETOMAXIMIZED");
	    WINDOWPLACEMENT wp;
	    ::ZeroMemory(&wp, sizeof(WINDOWPLACEMENT));
	    wp.length = sizeof(WINDOWPLACEMENT);
	    ::GetWindowPlacement(hwnd, &wp);
            if (!IsFocusableWindow()) {
                wp.showCmd = SW_SHOWMINNOACTIVE;
            } else {
                wp.showCmd = SW_SHOWMINIMIZED;
            }
	    wp.flags |= WPF_RESTORETOMAXIMIZED;
	    ::SetWindowPlacement(hwnd, &wp);
	}
	else {
	    DTRACE_PRINTLN("AwtFrame::Show(SW_SHOWMINIMIZED)");
            if (!IsFocusableWindow()) {
                ::ShowWindow(hwnd, SW_SHOWMINNOACTIVE);
            } else {
                ::ShowWindow(hwnd, SW_SHOWMINIMIZED);
            }
	}
    }
    else if (m_zoomed) {
	DTRACE_PRINTLN("AwtFrame::Show(SW_SHOWMAXIMIZED)");
        if (!IsFocusableWindow()) {
            ::ShowWindow(hwnd, SW_MAXIMIZE);
        } else {
            ::ShowWindow(hwnd, SW_SHOWMAXIMIZED);
        }
    }
    else if (m_isInputMethodWindow) {
	// Don't activate input methow window 
	DTRACE_PRINTLN("AwtFrame::Show(SW_SHOWNA)");
	::ShowWindow(hwnd, SW_SHOWNA);

	// After the input method window shown, we have to adjust the 
	// IME candidate window position. Here is why.
	// Usually, when IMM opens the candidate window, it sends WM_IME_NOTIFY w/
	// IMN_OPENCANDIDATE message to the awt component window. The 
	// awt component makes a Java call to acquire the text position
	// in order to show the candidate window just below the input method window. 
	// However, by the time it acquires the position, the input method window
	// hasn't been displayed yet, the position returned is just below 
	// the composed text and when the input method window is shown, it
	// will hide part of the candidate list. To fix this, we have to 
	// adjust the candidate window position after the input method window
	// is shown. See bug 5012944.
	AdjustCandidateWindowPos();
    }
    else {
	// Nor iconic, nor zoomed (handled above) - so use SW_RESTORE
	// to show in "normal" state regardless of whatever stale
	// state might the invisible window still has.
	DTRACE_PRINTLN("AwtFrame::Show(SW_RESTORE)");
        if (!IsFocusableWindow()) {
            ::ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        } else {
            ::ShowWindow(hwnd, SW_RESTORE);
        }
    }
}

void
AwtFrame::SendWindowStateEvent(int oldState, int newState)
{
    SendWindowEvent(java_awt_event_WindowEvent_WINDOW_STATE_CHANGED,
		    NULL, oldState, newState);
}

void
AwtFrame::ClearMaximizedBounds()
{
    m_maxBoundsSet = FALSE;
}

void AwtFrame::AdjustCandidateWindowPos()
{
    // This method should only be called if the current frame
    // is the input method window frame. 
    if (!m_isInputMethodWindow) {
        return;
    }

    RECT inputWinRec, focusWinRec;
    AwtComponent *comp = AwtComponent::GetComponent(AwtComponent::sm_focusOwner);
    if (comp == NULL) {
	return;
    }

    ::GetWindowRect(GetHWnd(), &inputWinRec);
    ::GetWindowRect(sm_focusOwner, &focusWinRec);

    LPARAM candType = comp->GetCandidateType();
    HWND defaultIMEWnd = ::ImmGetDefaultIMEWnd(GetHWnd());
    if (defaultIMEWnd == NULL) {
	return;
    }
    UINT bits = 1;
    // adjusts the candidate window position
    for (int iCandType = 0; iCandType < 32; iCandType++, bits<<=1) {
	if (candType & bits) {
	    CANDIDATEFORM cf;
	    cf.dwIndex = iCandType;
	    cf.dwStyle = CFS_CANDIDATEPOS;
	    // Since the coordinates are relative to the containing window,
	    // we have to calculate the coordinates as below.
	    cf.ptCurrentPos.x = inputWinRec.left - focusWinRec.left;
	    cf.ptCurrentPos.y = inputWinRec.bottom - focusWinRec.top;
            
	    // sends IMC_SETCANDIDATEPOS to IMM to move the candidate window.
	    ::SendMessage(defaultIMEWnd, WM_IME_CONTROL, IMC_SETCANDIDATEPOS, (LPARAM)&cf);
	}
    }
}

void
AwtFrame::SetMaximizedBounds(int x, int y, int w, int h)
{
    m_maxPos.x  = x;
    m_maxPos.y  = y;
    m_maxSize.x = w;
    m_maxSize.y = h;
    m_maxBoundsSet = TRUE;
}

MsgRouting AwtFrame::WmGetMinMaxInfo(LPMINMAXINFO lpmmi)
{
    //Firstly call AwtWindow's function
    MsgRouting r = AwtWindow::WmGetMinMaxInfo(lpmmi);

    //Then replace maxPos & maxSize if necessary
    if (!m_maxBoundsSet) {
        return r;
    }

    if (m_maxPos.x != java_lang_Integer_MAX_VALUE)
        lpmmi->ptMaxPosition.x = m_maxPos.x;
    if (m_maxPos.y != java_lang_Integer_MAX_VALUE)
        lpmmi->ptMaxPosition.y = m_maxPos.y;
    if (m_maxSize.x != java_lang_Integer_MAX_VALUE)
        lpmmi->ptMaxSize.x = m_maxSize.x;
    if (m_maxSize.y != java_lang_Integer_MAX_VALUE)
        lpmmi->ptMaxSize.y = m_maxSize.y;
    return mrConsume;
}

MsgRouting AwtFrame::WmSize(UINT type, int w, int h)
{
    currentWmSizeState = type;
    if (currentWmSizeState == SIZE_MINIMIZED) {
        UpdateSecurityWarningVisibility();
    }

    if (m_ignoreWmSize) {
        return mrDoDefault;
    }

    DTRACE_PRINTLN6("AwtFrame::WmSize: %dx%d,%s visible, state%s%s%s",
                  w, h,
                  ::IsWindowVisible(GetHWnd()) ? "" : " not",
                  m_iconic ? " iconic" : "",
                  m_zoomed ? " zoomed" : "",
                  m_iconic || m_zoomed ? "" : " normal");

    jint oldState = java_awt_Frame_NORMAL;
    if (m_iconic) {
	oldState |= java_awt_Frame_ICONIFIED;
    }
    if (m_zoomed) {
	oldState |= java_awt_Frame_MAXIMIZED_BOTH;
    }

    jint newState = java_awt_Frame_NORMAL;
    if (type == SIZE_MINIMIZED) {
      DTRACE_PRINTLN("AwtFrame::WmSize: SIZE_MINIMIZED");
	newState |= java_awt_Frame_ICONIFIED;
	if (m_zoomed) {
	    newState |= java_awt_Frame_MAXIMIZED_BOTH;
	}
	m_iconic = TRUE;
    }
    else if (type == SIZE_MAXIMIZED) {
      DTRACE_PRINTLN("AwtFrame::WmSize: SIZE_MAXIMIZED");
	newState |= java_awt_Frame_MAXIMIZED_BOTH;
	m_iconic = FALSE;
	m_zoomed = TRUE;
    }
    else if (type == SIZE_RESTORED) {
      DTRACE_PRINTLN("AwtFrame::WmSize: SIZE_RESTORED");
	m_iconic = FALSE;
	m_zoomed = FALSE;
    }

    jint changed = oldState ^ newState;
    if (changed != 0) {
      DTRACE_PRINTLN2("AwtFrame::WmSize: reporting state change %x -> %x",
                      oldState, newState);
	// report (de)iconification to old clients
	if (changed & java_awt_Frame_ICONIFIED) {
	    if (newState & java_awt_Frame_ICONIFIED) {
		SendWindowEvent(java_awt_event_WindowEvent_WINDOW_ICONIFIED);
	    } else {
		SendWindowEvent(java_awt_event_WindowEvent_WINDOW_DEICONIFIED);
	    }
	}

	// New (since 1.4) state change event
	SendWindowStateEvent(oldState, newState);
    }

    // If window is in iconic state, do not send COMPONENT_RESIZED event
    if (m_iconic) {
	return mrDoDefault;
    }

    return AwtWindow::WmSize(type, w, h);
}

MsgRouting AwtFrame::WmActivate(UINT nState, BOOL fMinimized, HWND opposite)
{
    // Process WM_ACTIVATE for EmbeddedFrame by request only
    if (IsEmbeddedFrame() && m_isEmbeddedFrameActivationRequest == FALSE) {
        return mrConsume;
    }
    m_isEmbeddedFrameActivationRequest = FALSE;

    jint type;
    BOOL doActivateFrame = TRUE;

    if (nState != WA_INACTIVE) {
        if (!::IsWindow(AwtWindow::GetModalBlocker(GetHWnd()))) {
            ::SetFocus(NULL); // The KeyboardFocusManager will set focus later
            type = java_awt_event_WindowEvent_WINDOW_GAINED_FOCUS;
            isAppActive = TRUE;
            AwtComponent::SetFocusedWindow(GetHWnd());
            
            /* 
             * Fix for 4823903.
             * If the window to be focused is actually not this Frame
             * and it's visible then send it WM_ACTIVATE.
             */
            if (m_actualFocusedWindow != NULL) {
                HWND hwnd = m_actualFocusedWindow->GetHWnd();
                
                if (hwnd != NULL && ::IsWindowVisible(hwnd)) {
                    
                    ::SendMessage(hwnd, WM_ACTIVATE, MAKEWPARAM(nState, fMinimized), (LPARAM)opposite);
                    doActivateFrame = FALSE;
                }      
                m_actualFocusedWindow = NULL;
            }
        } else {
            doActivateFrame = FALSE;
        }
    } else {
        if (!::IsWindow(AwtWindow::GetModalBlocker(opposite))) {
            // If deactivation happens because of press on grabbing
            // window - this is nonsense, since grabbing window is
            // assumed to have focus and watch for deactivation.  But
            // this can happen - if grabbing window is proxied Window,
            // with Frame keeping real focus for it.
            if (m_grabbedWindow != NULL) {
                if (m_grabbedWindow->GetHWnd() == opposite) {
                    // Do nothing
                } else {
                    // Normally, we would rather check that this ==
                    // grabbed window, and focus is leaving it -
                    // ungrab.  But since we know about proxied
                    // windows, we simply assume this is one of the
                    // known cases.
                    if (!m_grabbedWindow->IsOneOfOwnersOf((AwtWindow*)AwtComponent::GetComponent(opposite))) {
                        m_grabbedWindow->Ungrab();
                    }
                }
            }

            // If actual focused window is not this Frame
            if (AwtComponent::GetFocusedWindow() != GetHWnd()) {

                // Check that the Frame is going to be really inactive (i.e. the opposite is not its owned window)
                if (opposite != NULL) {
                    AwtWindow *wOpposite = (AwtWindow *)AwtComponent::GetComponent(opposite);

                    if (wOpposite != NULL &&
                        wOpposite->GetOwningFrameOrDialog() != this)
                    {
                        AwtWindow *window = (AwtWindow *)AwtComponent::GetComponent(AwtComponent::GetFocusedWindow());
    
                        // If actual focused window is one of Frame's owned windows
                        if (window != NULL && window->GetOwningFrameOrDialog() == this) {
                            m_actualFocusedWindow = window;    
                        }      
                    }
                }
            }  

            type = java_awt_event_WindowEvent_WINDOW_LOST_FOCUS;
            isAppActive = FALSE;
            AwtComponent::SetFocusedWindow(NULL);
        }
    }
    if (doActivateFrame) {
        SendWindowEvent(type, opposite);
    }
    return mrConsume;
}

MsgRouting AwtFrame::WmEnterMenuLoop(BOOL isTrackPopupMenu)
{
    if ( !isTrackPopupMenu ) {
	m_isMenuDropped = TRUE;
    }
    return mrDoDefault;
}

MsgRouting AwtFrame::WmExitMenuLoop(BOOL isTrackPopupMenu)
{
    if ( !isTrackPopupMenu ) {
	m_isMenuDropped = FALSE;
    }
    return mrDoDefault;
}

AwtMenuBar* AwtFrame::GetMenuBar()
{
    return menuBar;
}

void AwtFrame::SetMenuBar(AwtMenuBar* mb)
{
    menuBar = mb;
    if (mb == NULL) {
        // Remove existing menu bar, if any.
        ::SetMenu(GetHWnd(), NULL);
    } else {
        if (menuBar->GetHMenu() != NULL) {
            ::SetMenu(GetHWnd(), menuBar->GetHMenu());
        }
    }
}

MsgRouting AwtFrame::WmDrawItem(UINT ctrlId, DRAWITEMSTRUCT& drawInfo)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    // if the item to be redrawn is the menu bar, then do it
    AwtMenuBar* awtMenubar = GetMenuBar();
    if (drawInfo.CtlType == ODT_MENU && (awtMenubar != NULL) &&
        (::GetMenu( GetHWnd() ) == (HMENU)drawInfo.hwndItem) )
	{
		awtMenubar->DrawItem(drawInfo);
		return mrConsume;
    }

	return AwtComponent::WmDrawItem(ctrlId, drawInfo);
}

MsgRouting AwtFrame::WmMeasureItem(UINT ctrlId, MEASUREITEMSTRUCT& measureInfo)
{
	JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
	AwtMenuBar* awtMenubar = GetMenuBar();
	if ((measureInfo.CtlType == ODT_MENU) && (awtMenubar != NULL))
	{
		// AwtMenu instance is stored in itemData. Use it to check if this
		// menu is the menu bar.
		AwtMenu * pMenu = (AwtMenu *) measureInfo.itemData;
		DASSERT(pMenu != NULL);
		if ( pMenu == awtMenubar )
		{
			HWND hWnd = GetHWnd();
			HDC hDC = ::GetDC(hWnd);
			DASSERT(hDC != NULL);
			awtMenubar->MeasureItem(hDC, measureInfo);
			VERIFY(::ReleaseDC(hWnd, hDC));
			return mrConsume;
		}
	}

	return AwtComponent::WmMeasureItem(ctrlId, measureInfo);
}

MsgRouting AwtFrame::WmGetIcon(WPARAM iconType, LRESULT& retVal)
{
    //Workaround windows bug:
    //when reseting from specific icon to class icon
    //taskbar is not updated
    if (iconType <= 2 /*ICON_SMALL2*/) {
        retVal = (LRESULT)GetEffectiveIcon(iconType);
        return mrConsume;
    } else {
        return mrDoDefault;
    }
}

void AwtFrame::DoUpdateIcon()
{
    //Workaround windows bug:
    //when reseting from specific icon to class icon
    //taskbar is not updated
    HICON hIcon = GetEffectiveIcon(ICON_BIG);
    HICON hIconSm = GetEffectiveIcon(ICON_SMALL);
    SendMessage(WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
    SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
}

HICON AwtFrame::GetEffectiveIcon(int iconType) 
{
    BOOL smallIcon = ((iconType == ICON_SMALL) || (iconType == 2/*ICON_SMALL2*/));
    HICON hIcon = (smallIcon) ? GetHIconSm() : GetHIcon();
    if (hIcon == NULL) {
        hIcon = (smallIcon) ? AwtToolkit::GetInstance().GetAwtIconSm() : 
            AwtToolkit::GetInstance().GetAwtIcon();
    }
    return hIcon;
}
  
static BOOL keepOnMinimize(jobject peer) {
    static BOOL checked = FALSE; 
    static BOOL keep = FALSE;
    if (!checked) {
        keep = (JNU_GetStaticFieldByName(AwtToolkit::GetEnv(), NULL, 
            "sun/awt/windows/WFramePeer", "keepOnMinimize", "Z").z) == JNI_TRUE;
        checked = TRUE;
    }
    return keep;
}

MsgRouting AwtFrame::WmSysCommand(UINT uCmdType, int xPos, int yPos)
{
    // ignore any WM_SYSCOMMAND if this window is blocked by modal dialog
    if (::IsWindow(AwtWindow::GetModalBlocker(GetHWnd()))) {
        return mrConsume;
    }

    if (uCmdType == (SYSCOMMAND_IMM & 0xFFF0)){
        JNIEnv* env = AwtToolkit::GetEnv();
        JNU_CallMethodByName(env, NULL, m_peerObject, 
            "notifyIMMOptionChange", "()V");
        DASSERT(!safe_ExceptionOccurred(env));  
        return mrConsume;
    }
    if ((uCmdType == SC_MINIMIZE) && keepOnMinimize(m_peerObject)) {
        ::ShowWindow(GetHWnd(),SW_SHOWMINIMIZED);
        return mrConsume; 
    }			   
    return AwtWindow::WmSysCommand(uCmdType, xPos, yPos);
}

LRESULT AwtFrame::WinThreadExecProc(ExecuteArgs * args)
{
    switch( args->cmdId ) {
	case FRAME_SETMENUBAR:
	{
    	    jobject  mbPeer = (jobject)args->param1;

	    // cancel any currently dropped down menus
	    if (m_isMenuDropped) {
		SendMessage(WM_CANCELMODE);
	    }

	    if (mbPeer == NULL) {
		// Remove existing menu bar, if any
		SetMenuBar(NULL);
	    } else {
		JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
		AwtMenuBar* menuBar = (AwtMenuBar *)JNI_GET_PDATA(mbPeer);
		SetMenuBar(menuBar);
	    }
	    DrawMenuBar();
	    break;
	}

	default:
	    AwtWindow::WinThreadExecProc(args);
	    break;
    }

    return 0L;
}

////////////////////////////
// EmbeddedFrame focus stuff
////////////////////////////


void AwtFrame::ActivateFrameOnMouseDown()
{
    if (IsEmbeddedFrame()) {
        ActivateEmbeddedFrameHelper(AwtComponent::GetFocusedWindow());
    } else {
        /*
         * In case the frame is active but not focused
         * (that is an owned window is currently focused)
         * it should be sent an activating message.
         * This is needed to focus the frame when it's clicked
         * in an empty spot of its client area. See 6886678.
         */
        if (GetHWnd() == ::GetActiveWindow() &&
            GetHWnd() != AwtComponent::GetFocusedWindow())
        {
            SynthesizeWmActivate(TRUE, AwtComponent::GetFocusedWindow());
        }
    }
}

/*
 * hWndLostFocus - the component lossing focus
 * Returns TRUE if the EmbeddedFrame has been activated by this method (WM_SETFOCUS should be consumed),
 *         otherwise FALSE (WM_SETFOCUS should be processed further).
 */
BOOL AwtFrame::ActivateEmbeddedFrameOnSetFocus(HWND hWndLostFocus) 
{
    HWND oppositeToplevelHWnd = AwtComponent::GetTopLevelParentForWindow(hWndLostFocus);
    return ActivateEmbeddedFrameHelper(oppositeToplevelHWnd);
}

/*
 * oppositeToplevelHWnd - the toplevel lossing focus
 * Returns TRUE if the EmbeddedFrame has been activated by this method,
 *         otherwise FALSE.
 */
BOOL AwtFrame::ActivateEmbeddedFrameHelper(HWND oppositeToplevelHWnd)
{
    // If the EmbeddedFrame is not yet active, then this is either:
    // - requesting focus on smth in the EmbeddedFrame, or
    // - switching focus in the EmbeddedFrame from Location field by click, or
    // - switching focus in the EmbeddedFrame from the IE menu by Alt hitting (see 6374321).
    // In these cases we get WM_SETFOCUS without WM_ACTIVATE on the EmbeddedFrame.
    if (AwtComponent::GetFocusedWindow() != GetHWnd()) {

        // As we get WM_SETFOCUS from the native system we expect
        // the native toplevel be set to the active window.
        HWND activeWindowHWnd = ::GetActiveWindow();
        DASSERT(activeWindowHWnd == ::GetAncestor(GetHWnd(), GA_ROOT));
        
        // See 6538154.
        ::BringWindowToTop(activeWindowHWnd);
        ::SetForegroundWindow(activeWindowHWnd);

        SynthesizeWmActivate(TRUE, oppositeToplevelHWnd);

        return TRUE;
    }
    // If the EmbeddedFrame is already active.
    return FALSE;
}

/*
 * hWndGotFocus - the component gaining focus
 */
void AwtFrame::DeactivateEmbeddedFrameOnKillFocus(HWND hWndGotFocus)
{
    HWND oppositeToplevelHWnd = AwtComponent::GetTopLevelParentForWindow(hWndGotFocus);

    if (oppositeToplevelHWnd != AwtComponent::GetFocusedWindow()) {
        SynthesizeWmActivate(FALSE, oppositeToplevelHWnd);
    }
}

/*
 * Execute on Toolkit only.
 */
void AwtFrame::SynthesizeWmActivate(BOOL doActivate, HWND opposite)
{
    if (doActivate &&
        (!::IsWindowVisible(GetHWnd()) || ::IsIconic(::GetAncestor(GetHWnd(), GA_ROOT))))
    {
        // The activation is rejected if either:
        // - EmbeddedFrame is not visible or
        // - its topmost ancestor window is in minimized state.
        return;
    }
    m_isEmbeddedFrameActivationRequest = TRUE;
    ::SendMessage(GetHWnd(), WM_ACTIVATE, MAKEWPARAM(doActivate ? WA_ACTIVE : WA_INACTIVE, FALSE), (LPARAM) opposite);
}

void AwtFrame::_SynthesizeWmActivate(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SynthesizeWmActivateStruct *sas = (SynthesizeWmActivateStruct *)param;
    jobject self = sas->frame;
    jboolean doActivate = sas->doActivate;

    AwtFrame *frame = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    frame = (AwtFrame *)pData;

    frame->SynthesizeWmActivate(doActivate, NULL);
ret:
    env->DeleteGlobalRef(self);

    delete sas;
}

///////////////////////////////

jobject AwtFrame::_GetBoundsPrivate(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject self = (jobject)param;

    jobject result = NULL;
    AwtFrame *f = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    f = (AwtFrame *)pData;
    if (::IsWindow(f->GetHWnd()))
    {
        RECT rect;
        ::GetWindowRect(f->GetHWnd(), &rect);
        HWND parent = ::GetParent(f->GetHWnd());
        if (::IsWindow(parent))
        {
            POINT zero;
            zero.x = 0;
            zero.y = 0;
            ::ClientToScreen(parent, &zero);
            ::OffsetRect(&rect, -zero.x, -zero.y);
        }

        result = JNU_NewObjectByName(env, "java/awt/Rectangle", "(IIII)V",
            rect.left, rect.top, rect.bottom-rect.top, rect.right-rect.left);
    }
ret:
    env->DeleteGlobalRef(self);

    if (result != NULL)
    {
        jobject resultGlobalRef = env->NewGlobalRef(result);
        env->DeleteLocalRef(result);
        return resultGlobalRef;
    }
    else
    {
        return NULL;
    }
}

void AwtFrame::_SetState(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SetStateStruct *sss = (SetStateStruct *)param;
    jobject self = sss->frame;
    jint state = sss->state;

    AwtFrame *f = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    f = (AwtFrame *)pData;
    if (::IsWindow(f->GetHWnd()))
    {
        DASSERT(!IsBadReadPtr(f, sizeof(AwtFrame)));

        BOOL iconify = (state & java_awt_Frame_ICONIFIED) != 0;
        BOOL zoom = (state & java_awt_Frame_MAXIMIZED_BOTH)
			== java_awt_Frame_MAXIMIZED_BOTH;

        HWND hwnd = f->GetHWnd();
        BOOL focusable = f->IsFocusableWindow();

        DTRACE_PRINTLN4("WFramePeer.setState:%s%s ->%s%s",
                  f->isIconic() ? " iconic" : "",
                  f->isZoomed() ? " zoomed" : "",
                  iconify       ? " iconic" : "",
                  zoom          ? " zoomed" : "");

        if (::IsWindowVisible(hwnd)) {
            // Iconify first if necessary, so that for a complex state
            // transition zoom state is changed when we are iconified - to
            // reduce window flicker.
            if (!f->isIconic() && iconify) {
                if (focusable) {
                    ::ShowWindow(hwnd, SW_MINIMIZE);
                } else {
                    ::ShowWindow(hwnd, SW_SHOWMINNOACTIVE);
                }
            }

            // If iconified, handle zoom state change while/when in iconic state
            if (zoom != f->isZoomed()) {
                if (::IsIconic(hwnd)) {
                    // Arrange for window to be restored to specified state
                    WINDOWPLACEMENT wp;
                    ::ZeroMemory(&wp, sizeof(WINDOWPLACEMENT));
                    wp.length = sizeof(WINDOWPLACEMENT);
                    ::GetWindowPlacement(hwnd, &wp);

                    if (zoom) {
                        wp.flags |= WPF_RESTORETOMAXIMIZED;
                    } else {
                        wp.flags &= ~WPF_RESTORETOMAXIMIZED;
                    }
		    ::SetWindowPlacement(hwnd, &wp);
	        }
                else {
	            // Not iconified - just maximize it
                    if (focusable) {
                        ::ShowWindow(hwnd, zoom ? SW_SHOWMAXIMIZED : SW_RESTORE);
                    } else {
                        ::ShowWindow(hwnd, zoom ? SW_MAXIMIZE : SW_SHOWNOACTIVATE);
                    }
	        }
            }

            // Handle deiconify if necessary.
            if (f->isIconic() && !iconify) {
                if (focusable) {
                    ::ShowWindow(hwnd, SW_RESTORE);
                } else {
                    ::ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                }
	    }
        }
#ifdef DEBUG
        else {
          DTRACE_PRINTLN("  not visible, just recording the requested state");
        }
#endif

        f->setIconic(iconify);
        f->setZoomed(zoom);
    }
ret:
    env->DeleteGlobalRef(self);

    delete sss;
}

jint AwtFrame::_GetState(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject self = (jobject)param;

    jint result = java_awt_Frame_NORMAL;
    AwtFrame *f = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    f = (AwtFrame *)pData;
    if (::IsWindow(f->GetHWnd()))
    {
        DASSERT(!::IsBadReadPtr(f, sizeof(AwtFrame)));
        if (f->isIconic()) {
            result |= java_awt_Frame_ICONIFIED;
        }
        if (f->isZoomed()) {
            result |= java_awt_Frame_MAXIMIZED_BOTH;
        }

        DTRACE_PRINTLN2("WFramePeer.getState:%s%s",
                  f->isIconic() ? " iconic" : "",
                  f->isZoomed() ? " zoomed" : "");
    }
ret:
    env->DeleteGlobalRef(self);

    return result;
}

void AwtFrame::_SetMaximizedBounds(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SetMaximizedBoundsStruct *smbs = (SetMaximizedBoundsStruct *)param;
    jobject self = smbs->frame;
    int x = smbs->x;
    int y = smbs->y;
    int width = smbs->width;
    int height = smbs->height;

    AwtFrame *f = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    f = (AwtFrame *)pData;
    if (::IsWindow(f->GetHWnd()))
    {
        DASSERT(!::IsBadReadPtr(f, sizeof(AwtFrame)));
        f->SetMaximizedBounds(x, y, width, height);
    }
ret:
    env->DeleteGlobalRef(self);

    delete smbs;
}

void AwtFrame::_ClearMaximizedBounds(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    jobject self = (jobject)param;

    AwtFrame *f = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    f = (AwtFrame *)pData;
    if (::IsWindow(f->GetHWnd()))
    {
        DASSERT(!::IsBadReadPtr(f, sizeof(AwtFrame)));
        f->ClearMaximizedBounds();
    }
ret:
    env->DeleteGlobalRef(self);
}

void AwtFrame::_SetMenuBar(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SetMenuBarStruct *smbs = (SetMenuBarStruct *)param;
    jobject self = smbs->frame;
    jobject menubar = smbs->menubar;

    AwtFrame *f = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    f = (AwtFrame *)pData;
    if (::IsWindow(f->GetHWnd()))
    {
        ExecuteArgs args;
        args.cmdId = FRAME_SETMENUBAR;
        args.param1 = (LPARAM)menubar;
        f->WinThreadExecProc(&args);
    }
ret:
    env->DeleteGlobalRef(self);
    env->DeleteGlobalRef(menubar);

    delete smbs;
}

void AwtFrame::_SetIMMOption(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    SetIMMOptionStruct *sios = (SetIMMOptionStruct *)param;
    jobject self = sios->frame;
    jstring option = sios->option;

    int badAlloc = 0;
    LPCTSTR coption;
    LPTSTR empty = TEXT("InputMethod");
    AwtFrame *f = NULL;

    PDATA pData;
    JNI_CHECK_PEER_GOTO(self, ret);
    JNI_CHECK_NULL_GOTO(option, "IMMOption argument", ret);

    f = (AwtFrame *)pData;
    if (::IsWindow(f->GetHWnd()))
    {
        coption = JNU_GetStringPlatformChars(env, option, NULL);
        if (coption == NULL) 
        {
            badAlloc = 1;
        }
        if (!badAlloc)
        {
            HMENU hSysMenu = ::GetSystemMenu(f->GetHWnd(), FALSE);
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

void AwtFrame::_NotifyModalBlocked(void *param)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    NotifyModalBlockedStruct *nmbs = (NotifyModalBlockedStruct *)param;
    jobject self = nmbs->frame;
    jobject peer = nmbs->peer;
    jobject blockerPeer = nmbs->blockerPeer;
    jboolean blocked = nmbs->blocked;

    PDATA pData;

    pData = JNI_GET_PDATA(peer);
    AwtFrame *f = (AwtFrame *)pData;

    // dialog here may be NULL, for example, if the blocker is a native dialog
    // however, we need to install/unistall modal hooks anyway
    pData = JNI_GET_PDATA(blockerPeer);
    AwtDialog *d = (AwtDialog *)pData;

    if ((f != NULL) && ::IsWindow(f->GetHWnd()))
    {
        // get an HWND of the toplevel window this embedded frame is within
        HWND fHWnd = f->GetHWnd();
        while (::GetParent(fHWnd) != NULL) {
            fHWnd = ::GetParent(fHWnd);
        }
        // we must get a toplevel hwnd here, however due to some strange
        // behaviour of Java Plugin (a bug?) when running in IE at
        // this moment the embedded frame hasn't been placed into the
        // browser yet and fHWnd is not a toplevel, so we shouldn't install
        // the hook here
        if ((::GetWindowLong(fHWnd, GWL_STYLE) & WS_CHILD) == 0) {
            // if this toplevel is created in another thread, we should install
            // the modal hook into it to track window activation and mouse events
            DWORD fThread = ::GetWindowThreadProcessId(fHWnd, NULL);
            if (fThread != AwtToolkit::GetInstance().MainThread()) {
                // check if this thread has been already blocked
                BlockedThreadStruct *blockedThread = (BlockedThreadStruct *)sm_BlockedThreads.get((void *)fThread);
                if (blocked) {
                    if (blockedThread == NULL) {
                        blockedThread = new BlockedThreadStruct;
                        blockedThread->framesCount = 1;
                        blockedThread->modalHook = ::SetWindowsHookEx(WH_CBT, (HOOKPROC)AwtDialog::ModalFilterProc,
                                                                      0, fThread);
                        blockedThread->mouseHook = ::SetWindowsHookEx(WH_MOUSE, (HOOKPROC)AwtDialog::MouseHookProc_NonTT,
                                                                      0, fThread);
                        sm_BlockedThreads.put((void *)fThread, blockedThread);
                    } else {
                        blockedThread->framesCount++;
                    }
                } else {
                    // see the comment above: if Java Plugin behaviour when running in IE
                    // was right, blockedThread would be always not NULL here
                    if (blockedThread != NULL) {
                        DASSERT(blockedThread->framesCount > 0);
                        if ((blockedThread->framesCount) == 1) {
                            ::UnhookWindowsHookEx(blockedThread->modalHook);
                            ::UnhookWindowsHookEx(blockedThread->mouseHook);
                            sm_BlockedThreads.remove((void *)fThread);
                            delete blockedThread;
                        } else {
                            blockedThread->framesCount--;
                        }
                    }
                }
            }
        }
    }

    env->DeleteGlobalRef(self);
    env->DeleteGlobalRef(peer);
    env->DeleteGlobalRef(blockerPeer);

    delete nmbs;
}

/************************************************************************
 * WFramePeer native methods
 */

extern "C" {

/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    initIDs
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_java_awt_Frame_initIDs(JNIEnv *env, jclass cls)
{
    TRY;

    AwtFrame::stateID = env->GetFieldID(cls, "state", "I");
    DASSERT(AwtFrame::stateID != NULL);
        
    AwtFrame::undecoratedID = env->GetFieldID(cls,"undecorated","Z");
    DASSERT(AwtFrame::undecoratedID != NULL);    

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    setState
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WFramePeer_setState(JNIEnv *env, jobject self,
    jint state)
{
    TRY;

    SetStateStruct *sss = new SetStateStruct;
    sss->frame = env->NewGlobalRef(self);
    sss->state = state;

    AwtToolkit::GetInstance().SyncCall(AwtFrame::_SetState, sss);
    // global ref and sss are deleted in _SetState()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    getState
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WFramePeer_getState(JNIEnv *env, jobject self)
{
    TRY;

    jobject selfGlobalRef = env->NewGlobalRef(self);

    return static_cast<jint>(reinterpret_cast<INT_PTR>(AwtToolkit::GetInstance().SyncCall(
        (void*(*)(void*))AwtFrame::_GetState,
        (void *)selfGlobalRef)));
    // selfGlobalRef is deleted in _GetState()

    CATCH_BAD_ALLOC_RET(java_awt_Frame_NORMAL);
}


/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    setMaximizedBounds
 * Signature: (IIII)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WFramePeer_setMaximizedBounds(JNIEnv *env, jobject self,
    jint x, jint y, jint width, jint height)
{
    TRY;

    SetMaximizedBoundsStruct *smbs = new SetMaximizedBoundsStruct;
    smbs->frame = env->NewGlobalRef(self);
    smbs->x = x;
    smbs->y = y;
    smbs->width = width;
    smbs->height = height;

    AwtToolkit::GetInstance().SyncCall(AwtFrame::_SetMaximizedBounds, smbs);
    // global ref and smbs are deleted in _SetMaximizedBounds()

    CATCH_BAD_ALLOC;
}


/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    clearMaximizedBounds
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WFramePeer_clearMaximizedBounds(JNIEnv *env, jobject self)
{
    TRY;

    jobject selfGlobalRef = env->NewGlobalRef(self);

    AwtToolkit::GetInstance().SyncCall(AwtFrame::_ClearMaximizedBounds,
        (void *)selfGlobalRef);
    // selfGlobalRef is deleted in _ClearMaximizedBounds()

    CATCH_BAD_ALLOC;
}


/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    setMenuBar0
 * Signature: (Lsun/awt/windows/WMenuBarPeer;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WFramePeer_setMenuBar0(JNIEnv *env, jobject self,
					    jobject mbPeer)
{
    TRY;

    SetMenuBarStruct *smbs = new SetMenuBarStruct;
    smbs->frame = env->NewGlobalRef(self);
    smbs->menubar = env->NewGlobalRef(mbPeer);

    AwtToolkit::GetInstance().SyncCall(AwtFrame::_SetMenuBar, smbs);
    // global refs ans smbs are deleted in _SetMenuBar()

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    create
 * Signature: (Lsun/awt/windows/WComponentPeer;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WFramePeer_createAwtFrame(JNIEnv *env, jobject self,
                                               jobject parent)
{
    TRY;

    AwtToolkit::CreateComponent(self, parent,
				(AwtToolkit::ComponentFactory)
				AwtFrame::Create);
    PDATA pData;
    JNI_CHECK_PEER_CREATION_RETURN(self);

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    getSysMenuHeight
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WFramePeer_getSysMenuHeight(JNIEnv *env, jclass self)
{
    TRY;

    return ::GetSystemMetrics(SM_CYMENUSIZE);

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    pSetIMMOption
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WFramePeer_pSetIMMOption(JNIEnv *env, jobject self,
					       jstring option) 
{
    TRY;

    SetIMMOptionStruct *sios = new SetIMMOptionStruct;
    sios->frame = env->NewGlobalRef(self);
    sios->option = (jstring)env->NewGlobalRef(option);

    AwtToolkit::GetInstance().SyncCall(AwtFrame::_SetIMMOption, sios);
    // global refs and sios are deleted in _SetIMMOption()

    CATCH_BAD_ALLOC;
}

} /* extern "C" */


/************************************************************************
 * EmbeddedFrame native methods
 */

extern "C" {

/*
 * Class:     sun_awt_EmbeddedFrame
 * Method:    setPeer
 * Signature: (Ljava/awt/peer/ComponentPeer;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_EmbeddedFrame_setPeer(JNIEnv *env, jobject self, jobject lpeer)
{
    TRY;

    jclass cls;
    jfieldID fid;

    cls = env->GetObjectClass(self);
    fid = env->GetFieldID(cls, "peer", "Ljava/awt/peer/ComponentPeer;");
    env->SetObjectField(self, fid, lpeer);

    CATCH_BAD_ALLOC;
}

} /* extern "C" */


/************************************************************************
 * WEmbeddedFrame native methods
 */

extern "C" {

/*
 * Class:     sun_awt_windows_WFramePeer
 * Method:    initIDs
 * Signature: (Lsun/awt/windows/WMenuBarPeer;)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WEmbeddedFrame_initIDs(JNIEnv *env, jclass cls)
{
    TRY;

    AwtFrame::handleID = env->GetFieldID(cls, "handle", "J");
    DASSERT(AwtFrame::handleID != NULL);

    AwtFrame::activateEmbeddingTopLevelMID = env->GetMethodID(cls, "activateEmbeddingTopLevel", "()V");
    DASSERT(AwtFrame::activateEmbeddingTopLevelMID != NULL);

    CATCH_BAD_ALLOC;
}

JNIEXPORT void JNICALL
Java_sun_awt_windows_WEmbeddedFrame_notifyModalBlockedImpl(JNIEnv *env,
                                                           jobject self,
                                                           jobject peer,
                                                           jobject blockerPeer,
                                                           jboolean blocked)
{
    TRY;

    NotifyModalBlockedStruct *nmbs = new NotifyModalBlockedStruct;
    nmbs->frame = env->NewGlobalRef(self);
    nmbs->peer = env->NewGlobalRef(peer);
    nmbs->blockerPeer = env->NewGlobalRef(blockerPeer);
    nmbs->blocked = blocked;
    
    AwtToolkit::GetInstance().SyncCall(AwtFrame::_NotifyModalBlocked, nmbs);
    // global refs and nmbs are deleted in _NotifyModalBlocked()

    CATCH_BAD_ALLOC;
}

} /* extern "C" */


/************************************************************************
 * WEmbeddedFramePeer native methods
 */

extern "C" {

JNIEXPORT void JNICALL
Java_sun_awt_windows_WEmbeddedFramePeer_create(JNIEnv *env, jobject self,
					       jobject parent)
{
    TRY;

    JNI_CHECK_NULL_RETURN(self, "peer");
    AwtToolkit::CreateComponent(self, parent,
				(AwtToolkit::ComponentFactory)
				AwtFrame::Create);
    PDATA pData;
    JNI_CHECK_PEER_CREATION_RETURN(self);

    CATCH_BAD_ALLOC;
}

JNIEXPORT jobject JNICALL
Java_sun_awt_windows_WEmbeddedFramePeer_getBoundsPrivate(JNIEnv *env, jobject self)
{
    TRY;

    jobject result = (jobject)AwtToolkit::GetInstance().SyncCall(
        (void *(*)(void *))AwtFrame::_GetBoundsPrivate,
        env->NewGlobalRef(self));
    // global ref is deleted in _GetBoundsPrivate

    if (result != NULL)
    {
        jobject resultLocalRef = env->NewLocalRef(result);
        env->DeleteGlobalRef(result);
        return resultLocalRef;
    }
    else
    {
        return NULL;
    }

    CATCH_BAD_ALLOC_RET(NULL);
}

JNIEXPORT void JNICALL
Java_sun_awt_windows_WEmbeddedFramePeer_synthesizeWmActivate(JNIEnv *env, jobject self, jboolean doActivate)
{
    TRY;

    SynthesizeWmActivateStruct *sas = new SynthesizeWmActivateStruct;
    sas->frame = env->NewGlobalRef(self);
    sas->doActivate = doActivate;

    /*
     * WARNING: invoking this function without synchronization by m_Sync CriticalSection.
     * Taking this lock results in a deadlock.
     */
    AwtToolkit::GetInstance().InvokeFunction(AwtFrame::_SynthesizeWmActivate, sas);
    // global ref and sas are deleted in _SynthesizeWmActivate()

    CATCH_BAD_ALLOC;
}

} /* extern "C" */
