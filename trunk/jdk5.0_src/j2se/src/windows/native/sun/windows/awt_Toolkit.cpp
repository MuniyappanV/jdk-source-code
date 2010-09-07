/*
 * @(#)awt_Toolkit.cpp	1.183 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include <signal.h>

#if defined(_DEBUG) && defined(_MSC_VER) && _MSC_VER >= 1000
#include <crtdbg.h>
#endif

#define _JNI_IMPLEMENTATION_
#include "stdhdrs.h"
#include "awt_DrawingSurface.h"
#include "awt_AWTEvent.h"
#include "awt_Component.h"
#include "awt_Canvas.h"
#include "awt_Clipboard.h"
#include "awt_Frame.h"
#include "awt_Dialog.h"
#include "awt_Font.h"
#include "awt_Cursor.h"
#include "awt_InputEvent.h"
#include "awt_KeyEvent.h"
#include "awt_List.h"
#include "awt_Palette.h"
#include "awt_PopupMenu.h"
#include "awt_Toolkit.h"
#include "awt_DesktopProperties.h"
#include "awt_FileDialog.h"
#include "CmdIDList.h"
#include "awt_new.h"
#include "awt_Unicode.h"
#include "ddrawUtils.h"
#include "debug_trace.h"
#include "debug_mem.h"

#include <awt_DnDDT.h>
#include <awt_DnDDS.h>

#include <java_awt_Toolkit.h>
#include <java_awt_event_InputMethodEvent.h>
#include <java_awt_peer_ComponentPeer.h>

extern void initScreens(JNIEnv *env);
extern "C" void awt_dnd_initialize();
extern "C" void awt_dnd_uninitialize();
extern "C" void awt_clipboard_uninitialize(JNIEnv *env);
extern "C" BOOL g_bUserHasChangedInputLang;

extern CriticalSection windowMoveLock;
extern BOOL windowMoveLockHeld;

// Needed by JAWT: see awt_DrawingSurface.cpp.
extern jclass jawtVImgClass;
extern jclass jawtVSMgrClass;
extern jclass jawtComponentClass;
extern jclass jawtW32ossdClass;
extern jfieldID jawtPDataID;
extern jfieldID jawtSDataID;
extern jfieldID jawtSMgrID;

/************************************************************************
 * Utilities
 */

/* Initialize the Java VM instance variable when the library is 
   first loaded */
JavaVM *jvm;

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{
    TRY;

    jvm = vm;
    return JNI_VERSION_1_2;

    CATCH_BAD_ALLOC_RET(0);
}

extern "C" JNIEXPORT jboolean JNICALL AWTIsHeadless() {
    static JNIEnv *env = NULL;
    static jboolean isHeadless;
    jmethodID headlessFn;
    jclass graphicsEnvClass;

    if (env == NULL) {
        env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
        graphicsEnvClass = env->FindClass(
            "java/awt/GraphicsEnvironment");
        if (graphicsEnvClass == NULL) {
            return JNI_TRUE;
        }
        headlessFn = env->GetStaticMethodID(
            graphicsEnvClass, "isHeadless", "()Z");
        if (headlessFn == NULL) {
            return JNI_TRUE;
        }
        isHeadless = env->CallStaticBooleanMethod(graphicsEnvClass,
            headlessFn);
    }
    return isHeadless;
}

#define IDT_AWT_MOUSECHECK 0x101

static LPCTSTR szAwtToolkitClassName = TEXT("SunAwtToolkit");

UINT AwtToolkit::GetMouseKeyState()
{
    static BOOL mbSwapped = ::GetSystemMetrics(SM_SWAPBUTTON);
    UINT mouseKeyState = 0;

    if (HIBYTE(::GetKeyState(VK_CONTROL)))
        mouseKeyState |= MK_CONTROL;
    if (HIBYTE(::GetKeyState(VK_SHIFT)))
        mouseKeyState |= MK_SHIFT;
    if (HIBYTE(::GetKeyState(VK_LBUTTON)))
        mouseKeyState |= (mbSwapped ? MK_RBUTTON : MK_LBUTTON);
    if (HIBYTE(::GetKeyState(VK_RBUTTON)))
        mouseKeyState |= (mbSwapped ? MK_LBUTTON : MK_RBUTTON);
    if (HIBYTE(::GetKeyState(VK_MBUTTON)))
        mouseKeyState |= MK_MBUTTON;
    return mouseKeyState;
}

//
// Normal ::GetKeyboardState call only works if current thread has
// a message pump, so provide a way for other threads to get
// the keyboard state
//
void AwtToolkit::GetKeyboardState(PBYTE keyboardState)
{
    CriticalSection::Lock	l(AwtToolkit::GetInstance().m_lockKB);
    DASSERT(!IsBadWritePtr(keyboardState, KB_STATE_SIZE));
    memcpy(keyboardState, AwtToolkit::GetInstance().m_lastKeyboardState,
	   KB_STATE_SIZE);
}

void AwtToolkit::SetBusy(BOOL busy) {

    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

    static jclass awtAutoShutdownClass = NULL;
    static jmethodID notifyBusyMethodID = NULL;
    static jmethodID notifyFreeMethodID = NULL;

    if (awtAutoShutdownClass == NULL) {
        jclass awtAutoShutdownClassLocal = env->FindClass("sun/awt/AWTAutoShutdown");
        if (!JNU_IsNull(env, safe_ExceptionOccurred(env))) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        DASSERT(awtAutoShutdownClassLocal != NULL);
        if (awtAutoShutdownClassLocal == NULL) {
            return;
        }

        awtAutoShutdownClass = (jclass)env->NewGlobalRef(awtAutoShutdownClassLocal);
        env->DeleteLocalRef(awtAutoShutdownClassLocal);

        notifyBusyMethodID = env->GetStaticMethodID(awtAutoShutdownClass,
                                                    "notifyToolkitThreadBusy", "()V");
        if (!JNU_IsNull(env, safe_ExceptionOccurred(env))) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        notifyFreeMethodID = env->GetStaticMethodID(awtAutoShutdownClass,
                                                    "notifyToolkitThreadFree", "()V");
        if (!JNU_IsNull(env, safe_ExceptionOccurred(env))) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        DASSERT(notifyBusyMethodID != NULL);
        DASSERT(notifyFreeMethodID != NULL);
        if (notifyBusyMethodID == NULL || notifyFreeMethodID == NULL) {
            return;
        }
    } /* awtAutoShutdownClass == NULL*/

    if (busy) {
        env->CallStaticVoidMethod(awtAutoShutdownClass,
                                  notifyBusyMethodID);
    } else {
        env->CallStaticVoidMethod(awtAutoShutdownClass,
                                  notifyFreeMethodID);
    }

    if (!JNU_IsNull(env, safe_ExceptionOccurred(env))) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

BOOL AwtToolkit::activateKeyboardLayout(HKL hkl) {
    // This call should succeed in case of one of the following:
    // 1. Win 9x
    // 2. NT with that HKL already loaded
    HKL prev = ::ActivateKeyboardLayout(hkl, 0);

    // If the above call fails, try loading the layout in case of NT
    if ((prev == 0) && IS_NT) {
	
	// create input locale string, e.g., "00000409", from hkl.
	TCHAR inputLocale[9];
	TCHAR buf[9];
	_tcscpy(inputLocale, TEXT("00000000"));

    // 64-bit: ::LoadKeyboardLayout() is such a weird API - a string of
    // the hex value you want?!  Here we're converting our HKL value to
    // a string.  Hopefully there is no 64-bit trouble.
	_i64tot(reinterpret_cast<INT_PTR>(hkl), buf, 16);
	size_t len = _tcslen(buf);
	memcpy(&inputLocale[8-len], buf, len);

	// load and activate the keyboard layout
	hkl = ::LoadKeyboardLayout(inputLocale, 0);
	if (hkl != 0) {
	    prev = ::ActivateKeyboardLayout(hkl, 0);
	}
    }
    
    return (prev != 0);
}

/************************************************************************
 * Exported functions
 */

extern "C" BOOL APIENTRY DllMain(HANDLE hInstance, DWORD ul_reason_for_call, 
                                 LPVOID)
{
    // Don't use the TRY and CATCH_BAD_ALLOC_RET macros if we're detaching
    // the library. Doing so causes awt.dll to call back into the VM during
    // shutdown. This crashes the HotSpot VM.
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        TRY;
	AwtToolkit::GetInstance().SetModuleHandle((HMODULE)hInstance);
	CATCH_BAD_ALLOC_RET(FALSE);
	break;
    case DLL_PROCESS_DETACH:
#ifdef DEBUG
	DTrace_DisableMutex();
	DMem_DisableMutex();
#endif DEBUG
	// Release any resources that have not yet been released
	// Note that releasing DirectX objects is necessary for some
	// failure situations on win9x (such as the primary remaining
	// locked on application exit) but cannot be done during
	// PROCESS_DETACH on XP.  On NT and win2k calling this ends up
	// in a catch() clause in the calling function, but on XP
	// the process simply hangs during the release of the ddraw
	// device object.  Thus we check for NT here and do not bother
	// with the release on any NT flavored OS.  Note that XP is
	// based on NT, so the IS_NT check is valid for NT4, win2k, 
	// XP, and presumably XP follow-ons.
	if (!IS_NT) {
	    DDRelease();
	}
	break;
    }
    return TRUE;
}

/************************************************************************
 * AwtToolkit fields
 */

AwtToolkit AwtToolkit::theInstance;

/* ids for WToolkit fields accessed from native code */
jmethodID AwtToolkit::windowsSettingChangeMID;
jmethodID AwtToolkit::displayChangeMID;
/* ids for Toolkit methods */
jmethodID AwtToolkit::getDefaultToolkitMID;
jmethodID AwtToolkit::getFontMetricsMID;
jmethodID AwtToolkit::insetsMID;

/************************************************************************
 * JavaStringBuffer method
 */

JavaStringBuffer::JavaStringBuffer(JNIEnv *env, jstring jstr) {
    if (jstr != NULL) {
        int length = env->GetStringLength(jstr);
        buffer = new TCHAR[length + 1];
        LPCTSTR tmp = (LPCTSTR)JNU_GetStringPlatformChars(env, jstr, NULL);
	_tcscpy(buffer, tmp);
	JNU_ReleaseStringPlatformChars(env, jstr, tmp);
    } else {
        buffer = new TCHAR[1];
        buffer[0] = _T('\0');
    }
}


/************************************************************************
 * AwtToolkit methods
 */

AwtToolkit::AwtToolkit() {
    m_localPump = FALSE;
    m_mainThreadId = 0;
    m_toolkitHWnd = NULL;
    m_verbose = FALSE;
    m_isActive = TRUE;
    m_isDisposed = FALSE;

    m_vmSignalled = FALSE;

    m_isDynamicLayoutSet = FALSE;

    m_verifyComponents = FALSE;
    m_breakOnError = FALSE;

    m_breakMessageLoop = FALSE;
    m_messageLoopResult = 0;

    m_lastMouseOver = NULL;
    m_mouseDown = FALSE;

    m_hGetMessageHook = 0;
    m_timer = 0;

    m_cmdIDs = new AwtCmdIDList();
    m_pModalDialog = NULL;
    m_peer = NULL;
    m_dllHandle = NULL;

    m_displayChanged = FALSE;

    // XXX: keyboard mapping should really be moved out of AwtComponent
    AwtComponent::InitDynamicKeyMapTable();

    // initialize kb state array
    ::GetKeyboardState(m_lastKeyboardState);
}

AwtToolkit::~AwtToolkit() {
/*
 *  The code has been moved to AwtToolkit::Dispose() method.
 */
}

HWND AwtToolkit::CreateToolkitWnd(LPCTSTR name)
{
    HWND hwnd = CreateWindow(
	szAwtToolkitClassName,
	(LPCTSTR)name,	                  /* window name */
	WS_DISABLED,			  /* window style */
	-1, -1,				  /* position of window */
	0, 0,				  /* width and height */
	NULL, NULL, 			  /* hWndParent and hWndMenu */
	GetModuleHandle(),
	NULL);				  /* lpParam */
    DASSERT(hwnd != NULL);
    return hwnd;
}

BOOL AwtToolkit::Initialize(BOOL localPump) {
    AwtToolkit& tk = AwtToolkit::GetInstance();

    if (!tk.m_isActive || tk.m_mainThreadId != 0) {
	/* Already initialized. */
	return FALSE;
    }

    /* Register this toolkit's helper window */
    VERIFY(tk.RegisterClass() != NULL);

    // Set up operator new/malloc out of memory handler.
    NewHandler::init();

	//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
	// Bugs 4032109, 4047966, and 4071991 to fix AWT
	//	crash in 16 color display mode.  16 color mode is supported.  Less
	//	than 16 color is not.
	// creighto@eng.sun.com 1997-10-07
	//
	// Check for at least 16 colors
    HDC hDC = ::GetDC(NULL);
	if ((::GetDeviceCaps(hDC, BITSPIXEL) * ::GetDeviceCaps(hDC, PLANES)) < 4) {
		::MessageBox(NULL,
			     TEXT("Sorry, but this release of Java requires at least 16 colors"),
			     TEXT("AWT Initialization Error"),
			     MB_ICONHAND | MB_APPLMODAL);	
		::DeleteDC(hDC);
		JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
		JNU_ThrowByName(env, "java/lang/InternalError",
				"unsupported screen depth");
		return FALSE;
	}
    ::ReleaseDC(NULL, hDC);
	///////////////////////////////////////////////////////////////////////////

    tk.m_localPump = localPump;
    tk.m_mainThreadId = ::GetCurrentThreadId();

    /* 
     * Create the one-and-only toolkit window.  This window isn't
     * displayed, but is used to route messages to this thread.  
     */
    tk.m_toolkitHWnd = tk.CreateToolkitWnd(TEXT("theAwtToolkitWindow"));
    DASSERT(tk.m_toolkitHWnd != NULL);

    /*
     * Setup a GetMessage filter to watch all messages coming out of our 
     * queue from PreProcessMsg().
     */
    tk.m_hGetMessageHook = ::SetWindowsHookEx(WH_GETMESSAGE,
					      (HOOKPROC)GetMessageFilter,
					      0, tk.m_mainThreadId);

    awt_dnd_initialize();

    tk._begin_cursors();

    return TRUE;
}

BOOL AwtToolkit::Dispose() {
    DTRACE_PRINTLN("In AwtToolkit::Dispose()");

    AwtToolkit& tk = AwtToolkit::GetInstance();

    if (!tk.m_isActive || tk.m_mainThreadId != ::GetCurrentThreadId()) {
        return FALSE;
    }

    tk.m_isActive = FALSE;

    awt_dnd_uninitialize();
    awt_clipboard_uninitialize((JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2));

    AwtObjectList::Cleanup();
    AwtFont::Cleanup();

    HWND toolkitHWndToDestroy = tk.m_toolkitHWnd;
    tk.m_toolkitHWnd = 0;
    VERIFY(::DestroyWindow(toolkitHWndToDestroy) != NULL);

    tk.UnregisterClass();

    ::UnhookWindowsHookEx(tk.m_hGetMessageHook);

    tk.m_mainThreadId = 0;

    delete tk.m_cmdIDs;

    tk._end_cursors();

    tk.m_isDisposed = TRUE;

    return TRUE;
}

void AwtToolkit::SetDynamicLayout(BOOL dynamic) {
    m_isDynamicLayoutSet = dynamic;
}

BOOL AwtToolkit::IsDynamicLayoutSet() {
    return m_isDynamicLayoutSet;
}

BOOL AwtToolkit::IsDynamicLayoutSupported() {
    // SPI_GETDRAGFULLWINDOWS is only supported on Win95 if
    // Windows Plus! is installed.  Otherwise, box frame resize.
    BOOL fullWindowDragEnabled = FALSE;
    int result = 0;
    result = ::SystemParametersInfo(SPI_GETDRAGFULLWINDOWS, 0,
                                  &fullWindowDragEnabled, 0);

    return (fullWindowDragEnabled && (result != 0));
}

BOOL AwtToolkit::IsDynamicLayoutActive() {
    return (IsDynamicLayoutSet() && IsDynamicLayoutSupported());
}

void AwtToolkit::PrintWinVersion() {
    DWORD version = ::GetVersion();
    DTRACE_PRINT2("Windows Version is 0x%x = %ld : ", version, version);

    if (IS_WIN95) {
        if (IS_WIN98) {
            if (IS_WINME) {
                DTRACE_PRINTLN("os is Windows ME");
                return;
            }
            DTRACE_PRINTLN("os is Windows 98");
            return;
        }
        DTRACE_PRINTLN("os is Windows 95");
        return;
    } else if (IS_NT) {
        if (IS_WIN2000) {
            if (IS_WINXP) {
                DTRACE_PRINTLN("os is Windows XP");
                return;
            }
            DTRACE_PRINTLN("os is Windows 2000");
            return;
        }
        DTRACE_PRINTLN("os is Windows NT");
        return;
    } else {
        DTRACE_PRINTLN("Unrecognized operating system");
        return;
    }
}

ATOM AwtToolkit::RegisterClass() {
    WNDCLASS  wc;

    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = AwtToolkit::GetInstance().GetModuleHandle(),
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = szAwtToolkitClassName;

    ATOM ret = ::RegisterClass(&wc);
    DASSERT(ret != NULL);
    return ret;
}

void AwtToolkit::UnregisterClass() {
    VERIFY(::UnregisterClass(szAwtToolkitClassName, AwtToolkit::GetInstance().GetModuleHandle()));
}

/*
 * Structure holding the information to create a component. This packet is 
 * sent to the toolkit window.
 */
struct ComponentCreatePacket {
    void* hComponent;
    void* hParent;
    void (*factory)(void*, void*);
};

/*
 * Create an AwtXxxx component using a given factory function
 * Implemented by sending a message to the toolkit window to invoke the 
 * factory function from that thread
 */
void AwtToolkit::CreateComponent(void* component, void* parent, 
				 ComponentFactory compFactory, BOOL isParentALocalReference)
{
    JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);
    
    /* Since Local references are not valid in another Thread, we need to
       create a global reference before we send this to the Toolkit thread. 
       In some cases this method is called with parent being a native 
       malloced struct so we cannot and do not need to create a Global 
       Reference from it. This is indicated by isParentALocalReference */
           
    jobject gcomponent = env->NewGlobalRef((jobject)component); 
    jobject gparent;
    if (isParentALocalReference) gparent = env->NewGlobalRef((jobject)parent);
    ComponentCreatePacket ccp = { gcomponent,
                                  isParentALocalReference == TRUE ?  gparent : parent,
                                   compFactory };
    AwtToolkit::GetInstance().SendMessage(WM_AWT_COMPONENT_CREATE, 0,
					  (LPARAM)&ccp);
    env->DeleteGlobalRef(gcomponent);
    if (isParentALocalReference) env->DeleteGlobalRef(gparent);
}

/*
 * Destroy an HWND that was created in the toolkit thread. Can be used on 
 * Components and the toolkit window itself.
 */
void AwtToolkit::DestroyComponent(AwtComponent* comp)
{
    AwtToolkit& tk = AwtToolkit::GetInstance();
    if (comp == tk.m_lastMouseOver) {
        tk.m_lastMouseOver = NULL;
    }

    /* Don't filter any post-destroy messages, such as WM_NCDESTROY */
    comp->UnsubclassHWND();

    tk.SendMessage(WM_AWT_DESTROY_WINDOW, (WPARAM)comp->GetHWnd(), 0);
}

#ifndef SPY_MESSAGES
#define SpyWinMessage(hwin,msg,str)
#else
void SpyWinMessage(HWND hwnd, UINT message, LPCTSTR szComment);
#endif

/*
 * An AwtToolkit window is just a means of routing toolkit messages to here.
 */
LRESULT CALLBACK AwtToolkit::WndProc(HWND hWnd, UINT message, 
                                     WPARAM wParam, LPARAM lParam)
{
    TRY;

    JNIEnv *env = GetEnv();
    JNILocalFrame lframe(env, 10);

    SpyWinMessage(hWnd, message, TEXT("AwtToolkit"));

    /*
     * Awt widget creation messages are routed here so that all
     * widgets are created on the main thread.  Java allows widgets
     * to live beyond their creating thread -- by creating them on
     * the main thread, a widget can always be properly disposed.
     */
    switch (message) {
      case WM_AWT_EXECUTE_SYNC: {
	  AwtObject * 			object = (AwtObject *)wParam;
	  AwtObject::ExecuteArgs *	args = (AwtObject::ExecuteArgs *)lParam;
	  DASSERT(!IsBadReadPtr(object, sizeof(AwtObject)) );
	  DASSERT(!IsBadReadPtr(args, sizeof(AwtObject::ExecuteArgs)) );
	  return object->WinThreadExecProc(args);
      }
      case WM_AWT_COMPONENT_CREATE: {
          ComponentCreatePacket* ccp = (ComponentCreatePacket*)lParam;
          DASSERT(ccp->factory != NULL);
          DASSERT(ccp->hComponent != NULL);
          (*ccp->factory)(ccp->hComponent, ccp->hParent);
          return 0;
      }
      case WM_AWT_DESTROY_WINDOW: {
          /* Destroy widgets from this same thread that created them */
          VERIFY(::DestroyWindow((HWND)wParam) != NULL);
          return 0;
      }
      case WM_AWT_DISPOSE: {
          AwtObject *p = (AwtObject *)wParam;
          delete p;
          return 0;
      }
      case WM_SYSCOLORCHANGE: {

          jclass systemColorClass = env->FindClass("java/awt/SystemColor");
          DASSERT(systemColorClass);

          jmethodID mid = env->GetStaticMethodID(systemColorClass, "updateSystemColors", "()V");
          DASSERT(mid);

          env->CallStaticVoidMethod(systemColorClass, mid);

          /* FALL THROUGH - NO BREAK */
      }

      case WM_SETTINGCHANGE: {
          AwtWin32GraphicsDevice::ResetAllMonitorInfo();

          /* Upcall to WToolkit when user changes configuration.
           *
           * NOTE: there is a bug in Windows 98 and some older versions of
           * Windows NT (it seems to be fixed in NT4 SP5) where no
           * WM_SETTINGCHANGE is sent when any of the properties under
           * Control Panel -> Display are changed.  You must _always_ query
           * the system for these - you can't rely on cached values.
           */
	  jobject peer = AwtToolkit::GetInstance().m_peer;
	  if (peer != NULL) {
	      env->CallVoidMethod(peer, AwtToolkit::windowsSettingChangeMID);
	  }
	  return 0;
      }
      case WM_TIMER: {
          // Create an artifical MouseExit message if the mouse left to 
          // a non-java window (bad mouse!)
          POINT pt;
	  AwtToolkit& tk = AwtToolkit::GetInstance();
	  VERIFY(::GetCursorPos(&pt));
	  
	  HWND hWndOver = ::WindowFromPoint(pt);
	  AwtComponent * last_M;
	  if ( AwtComponent::GetComponent(hWndOver) == NULL && tk.m_lastMouseOver != NULL ) {
	      last_M = tk.m_lastMouseOver;
	      // translate point from screen to target window
	      MapWindowPoints(HWND_DESKTOP, last_M->GetHWnd(), &pt, 1);
	      last_M->SendMessage(WM_AWT_MOUSEEXIT,
				  GetMouseKeyState(), 
				  POINTTOPOINTS(pt));
	      tk.m_lastMouseOver = 0;
          }
          if (tk.m_lastMouseOver == NULL && tk.m_timer != 0) {
              VERIFY(::KillTimer(tk.m_toolkitHWnd, tk.m_timer));
	      tk.m_timer = 0;
	  }
          return 0;
      }
      case WM_AWT_POPUPMENU_SHOW: {
          AwtPopupMenu* popup = (AwtPopupMenu*)wParam;
          popup->Show(env, (jobject)lParam);
          return 0;
      }
      case WM_DESTROYCLIPBOARD: {
	  if (!AwtClipboard::IsGettingOwnership())
	      AwtClipboard::LostOwnership((JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2));
          return 0;
      }
      case WM_CHANGECBCHAIN: {
          AwtClipboard::WmChangeCbChain(wParam, lParam);
          return 0;
      }
      case WM_DRAWCLIPBOARD: {
          AwtClipboard::WmDrawClipboard((JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2), wParam, lParam);
          return 0;
      }
      case WM_AWT_LIST_SETMULTISELECT: {
          AwtList* list = (AwtList*)wParam;
          list->SetMultiSelect(static_cast<BOOL>(lParam));
          return 0;
      }

      // Special awt message to call Imm APIs.
      // ImmXXXX() API must be used in the main thread.
      // In other thread these APIs does not work correctly even if
      // it returs with no error. (This restriction is not documented)
      // So we must use thse messages to call these APIs in main thread.
      case WM_AWT_CREATECONTEXT: {
        return reinterpret_cast<LRESULT>(
            reinterpret_cast<void*>(ImmCreateContext()));
      }
      case WM_AWT_DESTROYCONTEXT: {
	  ImmDestroyContext((HIMC)wParam);
          return 0;
      }
      case WM_AWT_ASSOCIATECONTEXT: {
          AwtComponent *p = AwtComponent::GetComponent((HWND)wParam);
          p->ImmAssociateContext((HIMC)lParam);
          return 0;
      }
      case WM_AWT_ENDCOMPOSITION: {
	  /*right now we just cancel the composition string
	  may need to commit it in the furture
	  Changed to commit it according to the flag 10/29/98*/
          ImmNotifyIME((HIMC)wParam, NI_COMPOSITIONSTR,
	               (lParam ? CPS_COMPLETE : CPS_CANCEL), 0);
          return 0;
      }
      case WM_AWT_SETCONVERSIONSTATUS: {
	  DWORD cmode;
	  DWORD smode;
	  ImmGetConversionStatus((HIMC)wParam, (LPDWORD)&cmode, (LPDWORD)&smode);
	  ImmSetConversionStatus((HIMC)wParam, (DWORD)LOWORD(lParam), smode);
	  return 0;
      }
      case WM_AWT_GETCONVERSIONSTATUS: {
	  DWORD cmode;
	  DWORD smode;
	  ImmGetConversionStatus((HIMC)wParam, (LPDWORD)&cmode, (LPDWORD)&smode);
	  return cmode;
      }
      case WM_AWT_ACTIVATEKEYBOARDLAYOUT: {
          if (wParam && g_bUserHasChangedInputLang) {
	      // Input language has been changed since the last WInputMethod.getNativeLocale()
	      // call.  So let's honor the user's selection.
	      // Note: we need to check this flag inside the toolkit thread to synchronize access
	      // to the flag.
	      return FALSE;
	  }
	 
          if (lParam == (LPARAM)::GetKeyboardLayout(0)) {
	      // already active
	      return FALSE;
	  }
	 
	  // Since ActivateKeyboardLayout does not post WM_INPUTLANGCHANGEREQUEST,
	  // we explicitly need to do the same thing here.
	  static BYTE keyboardState[AwtToolkit::KB_STATE_SIZE];
	  AwtToolkit::GetKeyboardState(keyboardState);
	  WORD ignored;
	  ::ToAscii(VK_SPACE, ::MapVirtualKey(VK_SPACE, 0),
		    keyboardState, &ignored, 0);

	  return (LRESULT)activateKeyboardLayout((HKL)lParam);
      }
      case WM_AWT_OPENCANDIDATEWINDOW: {
          ((AwtComponent *)wParam)->OpenCandidateWindow(LOWORD(lParam), HIWORD(lParam));
	  return 0;
      }


      /*
       * send this message via ::SendMessage() and the MPT will acquire the
       * HANDLE synchronized with the sender's thread. The HANDLE must be
       * signalled or deadlock may occur between the MPT and the caller.
       */

      case WM_AWT_WAIT_FOR_SINGLE_OBJECT: {
	return ::WaitForSingleObject((HANDLE)lParam, INFINITE);
      }
      case WM_AWT_INVOKE_METHOD: {
	return (LRESULT)(*(void*(*)(void*))wParam)((void *)lParam);
      }
      case WM_AWT_INVOKE_VOID_METHOD: {
	return (LRESULT)(*(void*(*)(void))wParam)();
      }

      case WM_AWT_SETOPENSTATUS: {
	  ImmSetOpenStatus((HIMC)wParam, (BOOL)lParam);
	  return 0;
      }
      case WM_AWT_GETOPENSTATUS: {
	  return (DWORD)ImmGetOpenStatus((HIMC)wParam);
      }
      case WM_DISPLAYCHANGE: {
	  AwtCursor::DirtyAllCustomCursors();

	  // Reinitialize screens
	  initScreens(env);

	  // Invalidate current DDraw object; the object must be recreated
	  // when we first try to create a new DDraw surface.  Note that we
	  // don't recreate the ddraw object directly here because of 
	  // multi-threading issues; we'll just leave that to the first
	  // time an object tries to create a DDraw surface under the new
	  // display depth (which will happen after the displayChange event
	  // propagation at the end of this case).
          if (DDCanReplaceSurfaces(NULL)) {
	      DDInvalidateDDInstance(NULL);
      	  }

	  // Notify Java side - call WToolkit.displayChanged()
	  jclass clazz = env->FindClass("sun/awt/windows/WToolkit");
	  env->CallStaticVoidMethod(clazz, AwtToolkit::displayChangeMID);

	  GetInstance().m_displayChanged = TRUE;

	  ::PostMessage(HWND_BROADCAST, WM_PALETTEISCHANGING, NULL, NULL);
	  break;
      }
      case WM_AWT_SETCURSOR: {
	  ::SetCursor((HCURSOR)wParam);
	  return TRUE;
      }
      /* Session management */
      case WM_QUERYENDSESSION: {
	  /* Shut down cleanly */
	  if (JVM_RaiseSignal(SIGTERM)) {
              AwtToolkit::GetInstance().m_vmSignalled = TRUE;
          }
	  return TRUE;
      }
      case WM_ENDSESSION: {
	  // Keep pumping messages until the shutdown sequence halts the VM,
	  // or we exit the MessageLoop because of a WM_QUIT message
	  AwtToolkit& tk = AwtToolkit::GetInstance();

          // if WM_QUERYENDSESSION hasn't successfully raised SIGTERM
          // we ignore the ENDSESSION message
          if (!tk.m_vmSignalled) {
              return 0;
          }
	  tk.MessageLoop(AwtToolkit::PrimaryIdleFunc, 
			 AwtToolkit::CommonPeekMessageFunc);

	  // Dispose here instead of in eventLoop so that we don't have
	  // to return from the WM_ENDSESSION handler.
	  tk.Dispose();

	  // Never return. The VM will halt the process.
	  hang_if_shutdown();

	  // Should never get here.
	  DASSERT(FALSE);
      }

    }
    return DefWindowProc(hWnd, message, wParam, lParam);

    CATCH_BAD_ALLOC_RET(0);
}

LRESULT CALLBACK AwtToolkit::GetMessageFilter(int code,
					      WPARAM wParam, LPARAM lParam)
{
    TRY;

    if (code >= 0 && wParam == PM_REMOVE && lParam != 0) {
       if (AwtToolkit::GetInstance().PreProcessMsg(*(MSG*)lParam) !=
	       mrPassAlong) {
           /* PreProcessMsg() wants us to eat it */
           ((MSG*)lParam)->message = WM_NULL;  
       }
    }
    return ::CallNextHookEx(AwtToolkit::GetInstance().m_hGetMessageHook, code, 
			    wParam, lParam);

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * The main message loop
 */

const int AwtToolkit::EXIT_ENCLOSING_LOOP      = 0;
const int AwtToolkit::EXIT_ALL_ENCLOSING_LOOPS = -1;


/**
 * Called upon event idle to ensure that we have released any
 * CriticalSections that we took during window event processing.
 *
 * Note that this gets used more often than you would think; some
 * window moves actually happen over more than one event burst.  So,
 * for example, we might get a WINDOWPOSCHANGING event, then we 
 * idle and release the lock here, then eventually we get the
 * WINDOWPOSCHANGED event.
 *
 * This method may be called from WToolkit.embeddedEventLoopIdleProcessing 
 * if there is a separate event loop that must do the same CriticalSection 
 * check.
 * 
 * See bug #4526587 for more information.
 */
void VerifyWindowMoveLockReleased()
{
    if (windowMoveLockHeld) {
	windowMoveLockHeld = FALSE;
	windowMoveLock.Leave();
    }
}

UINT 
AwtToolkit::MessageLoop(IDLEPROC lpIdleFunc, 
			PEEKMESSAGEPROC lpPeekMessageFunc)
{
    DTRACE_PRINTLN("AWT event loop started");

    DASSERT(lpIdleFunc != NULL);
    DASSERT(lpPeekMessageFunc != NULL);

    m_messageLoopResult = 0;
    while (!m_breakMessageLoop) {            

	(*lpIdleFunc)();

        PumpWaitingMessages(lpPeekMessageFunc); /* pumps waiting messages */

	// Catch problems with windowMoveLock critical section.  In case we
	// misunderstood the way windows processes window move/resize
	// events, we don't want to hold onto the windowMoveLock CS forever.
	// If we've finished processing events for now, release the lock
	// if held.
	VerifyWindowMoveLockReleased();
    }
    if (m_messageLoopResult == EXIT_ALL_ENCLOSING_LOOPS)
	::PostQuitMessage(EXIT_ALL_ENCLOSING_LOOPS);
    m_breakMessageLoop = FALSE;

    DTRACE_PRINTLN("AWT event loop ended");

    return m_messageLoopResult;
}

/*
 * Exit the enclosing message loop(s).
 *
 * The message will be ignored if Windows is currently is in an internal
 * message loop (such as a scroll bar drag). So we first send IDCANCEL and
 * WM_CANCELMODE messages to every Window on the thread.
 */
static BOOL CALLBACK CancelAllThreadWindows(HWND hWnd, LPARAM)
{
    TRY;

    ::SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), (LPARAM)hWnd);
    ::SendMessage(hWnd, WM_CANCELMODE, 0, 0);

    return TRUE;

    CATCH_BAD_ALLOC_RET(FALSE);
}

static void DoQuitMessageLoop(void* param) {
    int status = *static_cast<int*>(param);

    AwtToolkit::GetInstance().QuitMessageLoop(status);
}

void AwtToolkit::QuitMessageLoop(int status) {
    /*
     * Fix for 4623377.
     * Reinvoke QuitMessageLoop on the toolkit thread, so that 
     * m_breakMessageLoop is accessed on a single thread.
     */
    if (!AwtToolkit::IsMainThread()) {
        InvokeFunction(DoQuitMessageLoop, &status);
        return;
    }

    /*
     * Fix for BugTraq ID 4445747.
     * EnumThreadWindows() is very slow during dnd on Win9X/ME.
     * This call is unnecessary during dnd, since we postpone processing of all
     * messages that can enter internal message loop until dnd is over.
     */
      if (status == EXIT_ALL_ENCLOSING_LOOPS) {
          ::EnumThreadWindows(MainThread(), (WNDENUMPROC)CancelAllThreadWindows,
                              0);
      }

    /*
     * Fix for 4623377.
     * Modal loop may not exit immediatelly after WM_CANCELMODE, so it still can
     * eat WM_QUIT message and the nested message loop will never exit.
     * The fix is to use AwtToolkit instance variables instead of WM_QUIT to
     * guarantee that we exit from the nested message loop when any possible
     * modal loop quits. In this case CancelAllThreadWindows is needed only to
     * ensure that the nested message loop exits quickly and doesn't wait until
     * a possible modal loop completes.
     */
    m_breakMessageLoop = TRUE;
    m_messageLoopResult = status;

    /*
     * Fix for 4683602.
     * Post an empty message, to wake up the toolkit thread 
     * if it is currently in WaitMessage(), 
     */
    PostMessage(WM_NULL);
}

/*
 * Called by the message loop to pump the message queue when there are
 * messages waiting. Can also be called anywhere to pump messages.
 */
BOOL AwtToolkit::PumpWaitingMessages(PEEKMESSAGEPROC lpPeekMessageFunc)
{
    MSG  msg;
    BOOL foundOne = FALSE;

    DASSERT(lpPeekMessageFunc != NULL);

    while (!m_breakMessageLoop && (*lpPeekMessageFunc)(msg)) {
        foundOne = TRUE;
        if (msg.message == WM_QUIT) {
            m_breakMessageLoop = TRUE;
            m_messageLoopResult = static_cast<UINT>(msg.wParam);
            if (m_messageLoopResult == EXIT_ALL_ENCLOSING_LOOPS)
		::PostQuitMessage(static_cast<int>(msg.wParam));  // make sure all loops exit
            break;
        }
        else if (msg.message != WM_NULL) {
	    /*
	     * The AWT in standalone mode (that is, dynamically loaded from the
	     * Java VM) doesn't have any translation tables to worry about, so
	     * TranslateAccelerator isn't called.
	     */

            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }
    return foundOne;
}

VOID CALLBACK
AwtToolkit::PrimaryIdleFunc() {
    AwtToolkit::SetBusy(FALSE);
    ::WaitMessage();               /* allow system to go idle */
    AwtToolkit::SetBusy(TRUE);
}

VOID CALLBACK
AwtToolkit::SecondaryIdleFunc() {
    ::WaitMessage();               /* allow system to go idle */
}

BOOL 
AwtToolkit::CommonPeekMessageFunc(MSG& msg) {
    return ::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
}

/*
 * Perform pre-processing on a message before it is translated &
 * dispatched.  Returns true to eat the message
 */
BOOL AwtToolkit::PreProcessMsg(MSG& msg)
{
    /*
     * Offer preprocessing first to the target component, then call out to
     * specific mouse and key preprocessor methods
     */
    AwtComponent* p = AwtComponent::GetComponent(msg.hwnd);
    if (p && p->PreProcessMsg(msg) == mrConsume)
        return TRUE;

    if ((msg.message >= WM_MOUSEFIRST && msg.message <= WM_AWT_MOUSELAST) ||
        (IS_WIN95 && !IS_WIN98 &&
                                msg.message == AwtComponent::Wheel95GetMsg()) ||
        (msg.message >= WM_NCMOUSEMOVE && msg.message <= WM_NCMBUTTONDBLCLK)) {
        if (PreProcessMouseMsg(p, msg)) {
            return TRUE;
        }
    }
    else if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) {
        if (PreProcessKeyMsg(p, msg))
            return TRUE;
    }
    return FALSE;
}

BOOL AwtToolkit::PreProcessMouseMsg(AwtComponent* p, MSG& msg)
{
    WPARAM mouseWParam;
    LPARAM mouseLParam;

    /*
     * Fix for BugTraq ID 4395290.
     * Do not synthesize mouse enter/exit events during drag-and-drop, 
     * since it messes up LightweightDispatcher.
     */
    if (AwtDropTarget::IsLocalDnD()) {
        return FALSE;
    }

    if (msg.message >= WM_MOUSEFIRST && msg.message <= WM_AWT_MOUSELAST ||
        (IS_WIN95 && !IS_WIN98 && msg.message == AwtComponent::Wheel95GetMsg()))
    {
        mouseWParam = msg.wParam;
        mouseLParam = msg.lParam;
    } else {
        mouseWParam = GetMouseKeyState();
    }

    /*
     * Get the window under the mouse, as it will be different if its
     * captured.
     */
    DWORD dwCurPos = ::GetMessagePos();
    DWORD dwScreenPos = dwCurPos;
    POINT curPos;
    curPos.x = LOWORD(dwCurPos);
    curPos.y = HIWORD(dwCurPos);
    HWND hWndFromPoint = ::WindowFromPoint(curPos);
    // hWndFromPoint == 0 if mouse is over a scrollbar
    AwtComponent* mouseComp = 
        AwtComponent::GetComponent(hWndFromPoint);
    // Need extra copies for non-client area issues
    AwtComponent* mouseWheelComp = mouseComp;
    HWND hWndForWheel = hWndFromPoint;

    // If the point under the mouse isn't in the client area,
    // ignore it to maintain compatibility with Solaris (#4095172)
    RECT windowRect;
    ::GetClientRect(hWndFromPoint, &windowRect);
    POINT topLeft;
    topLeft.x = 0;
    topLeft.y = 0;
    ::ClientToScreen(hWndFromPoint, &topLeft);
    windowRect.top += topLeft.y;
    windowRect.bottom += topLeft.y;
    windowRect.left += topLeft.x;
    windowRect.right += topLeft.x;
    if ((curPos.y < windowRect.top) ||
        (curPos.y >= windowRect.bottom) ||
        (curPos.x < windowRect.left) ||
        (curPos.x >= windowRect.right)) {
        mouseComp = NULL;
	hWndFromPoint = NULL;
    }

    /*
     * Look for mouse transitions between windows & create 
     * MouseExit & MouseEnter messages 
     */
    if (mouseComp != m_lastMouseOver) {
	/*
	 * Send the messages right to the windows so that they are in 
	 * the right sequence.
	 */
        if (m_lastMouseOver) {
            dwCurPos = dwScreenPos;
            curPos.x = LOWORD(dwCurPos);
            curPos.y = HIWORD(dwCurPos);
            ::MapWindowPoints(HWND_DESKTOP, m_lastMouseOver->GetHWnd(),
                              &curPos, 1);
            mouseLParam = MAKELPARAM((WORD)curPos.x, (WORD)curPos.y);
            m_lastMouseOver->SendMessage(WM_AWT_MOUSEEXIT, mouseWParam, 
					 mouseLParam);
        }
        if (mouseComp) {
                dwCurPos = dwScreenPos;
                curPos.x = LOWORD(dwCurPos);
                curPos.y = HIWORD(dwCurPos);
                ::MapWindowPoints(HWND_DESKTOP, mouseComp->GetHWnd(),
                                  &curPos, 1);
                mouseLParam = MAKELPARAM((WORD)curPos.x, (WORD)curPos.y);
            mouseComp->SendMessage(WM_AWT_MOUSEENTER, mouseWParam,
				   mouseLParam);
        }
        m_lastMouseOver = mouseComp;
    }

    /*
     * For MouseWheelEvents, hwnd must be changed to be the Component under
     * the mouse, not the Component with the input focus.
     */

    if (msg.message == WM_MOUSEWHEEL &&
        mouseWheelComp != NULL) { //i.e. mouse is over client area for this
                                  //window
        msg.hwnd = hWndForWheel;
    }
    else if (IS_WIN95 && !IS_WIN98 &&
             msg.message == AwtComponent::Wheel95GetMsg() &&
             mouseWheelComp != NULL) {

        // On Win95, mouse wheels are _always_ delivered to the top level
        // Frame.  Default behavior only takes place if the message's hwnd
        // remains that of the Frame.  We only want to change the hwnd if
        // we're changing it to a Component that DOESN'T handle the 
        // mousewheel natively.

        if (!mouseWheelComp->InheritsNativeMouseWheelBehavior()) {
            DTRACE_PRINTLN("AwtT::PPMM: changing hwnd on 95");
            msg.hwnd = hWndForWheel;
        }
    }

    /*
     * Make sure we get at least one last chance to check for transitions 
     * before we sleep
     */
    if (m_lastMouseOver && !m_timer) {
        m_timer = ::SetTimer(m_toolkitHWnd, IDT_AWT_MOUSECHECK, 200, 0);
    }
    return FALSE;  /* Now go ahead and process current message as usual */
}

BOOL AwtToolkit::PreProcessKeyMsg(AwtComponent* p, MSG& msg)
{
    // get keyboard state for use in AwtToolkit::GetKeyboardState
    CriticalSection::Lock	l(m_lockKB);
    ::GetKeyboardState(m_lastKeyboardState);
    return FALSE;
}

UINT AwtToolkit::CreateCmdID(AwtObject* object) 
{ 
    return m_cmdIDs->Add(object);
}

void AwtToolkit::RemoveCmdID(UINT id)
{
    m_cmdIDs->Remove(id);
}

AwtObject* AwtToolkit::LookupCmdID(UINT id)
{
    return m_cmdIDs->Lookup(id);
}

HICON AwtToolkit::GetAwtIcon()
{
    return ::LoadIcon(GetModuleHandle(), TEXT("AWT_ICON"));
}

void AwtToolkit::SetHeapCheck(long flag) {
    if (flag) {
        printf("heap checking not supported with this build\n");
    }
}

/* cursor support 
 */
void AwtToolkit::_begin_cursors() {
  m_nCustomCursor = FALSE;
  _detect_themes();
}
void AwtToolkit::_end_cursors() {
  m_nCustomCursor = FALSE;
}
void AwtToolkit::_detect_themes() {
  HKEY regKey = NULL;
  
  m_nHasThemes = FALSE;
  
  if( ERROR_SUCCESS == ::RegOpenKeyEx(
				      HKEY_CURRENT_USER,
				      TEXT("Control Panel\\Cursors"),
				      0,
				      KEY_READ,
				      &regKey ) ) {
    TCHAR nBuffer[0xFF];
    DWORD buffLen = 0xFF;
    
    if( ERROR_SUCCESS == ::RegQueryValueEx(
					   regKey,
					   TEXT("Arrow"),
					   0,0,(LPBYTE)nBuffer,
					   &buffLen) ) {
      HCURSOR hC;
      if (_tcsstr(nBuffer, TEXT("%SYSTEMROOT%"))) {
        TCHAR nBufferCorrect[0xff];
        _tcscpy(nBufferCorrect, _tgetenv(TEXT("SYSTEMROOT")));
        _tcscat(nBufferCorrect, (nBuffer + _tcslen(TEXT("%SYSTEMROOT%"))));
        hC = ::LoadCursorFromFile(nBufferCorrect);
      } else {
        hC = ::LoadCursorFromFile(nBuffer);
      }
      if( NULL != hC ) {
	m_nHasThemes = TRUE;
	::DestroyCursor(hC);
      }
    }
    ::RegCloseKey(regKey);
  }
}

void throw_if_shutdown(void) throw (awt_toolkit_shutdown)
{
    AwtToolkit::GetInstance().VerifyActive();
}
void hang_if_shutdown(void)
{
    try {
        AwtToolkit::GetInstance().VerifyActive();
    } catch (awt_toolkit_shutdown&) {
        // Never return. The VM will halt the process.
        ::WaitForSingleObject(::CreateEvent(NULL, TRUE, FALSE, NULL),
                              INFINITE);
        // Should never get here.
        DASSERT(FALSE);
    }
}

/*
 * Returns a reference to the class java.awt.Component.
 */
jclass
getComponentClass(JNIEnv *env)
{
    static jclass componentCls = NULL;

    // get global reference of java/awt/Component class (run only once)
    if (componentCls == NULL) {
        jclass componentClsLocal = env->FindClass("java/awt/Component");
        DASSERT(componentClsLocal != NULL);
        if (componentClsLocal == NULL) {
            /* exception already thrown */
            return NULL;
        }
	componentCls = (jclass)env->NewGlobalRef(componentClsLocal);
        env->DeleteLocalRef(componentClsLocal);
    }
    return componentCls;
}


/*
 * Returns a reference to the class java.awt.MenuComponent.
 */
jclass
getMenuComponentClass(JNIEnv *env)
{
    static jclass menuComponentCls = NULL;

    // get global reference of java/awt/MenuComponent class (run only once)
    if (menuComponentCls == NULL) {
        jclass menuComponentClsLocal = env->FindClass("java/awt/MenuComponent");
        DASSERT(menuComponentClsLocal != NULL);
        if (menuComponentClsLocal == NULL) {
            /* exception already thrown */
            return NULL;
        }
	menuComponentCls = (jclass)env->NewGlobalRef(menuComponentClsLocal);
        env->DeleteLocalRef(menuComponentClsLocal);
    }
    return menuComponentCls;
}

JNIEnv* AwtToolkit::m_env;
HANDLE AwtToolkit::m_thread;

void AwtToolkit::SetEnv(JNIEnv *env) {
    if (m_env != NULL) { // If already cashed (by means of embeddedInit() call).
        return;
    }
    m_thread = GetCurrentThread();
    m_env = env;
}

JNIEnv* AwtToolkit::GetEnv() {
    return (m_env == NULL || m_thread != GetCurrentThread()) ?
        (JNIEnv*)JNU_GetEnv(jvm, JNI_VERSION_1_2) : m_env;
}

/************************************************************************
 * Toolkit native methods
 */

extern "C" {
  
/*
 * Class:     java_awt_Toolkit
 * Method:    initIDs
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_java_awt_Toolkit_initIDs(JNIEnv *env, jclass cls) {
    TRY;

    AwtToolkit::getDefaultToolkitMID = 
	env->GetStaticMethodID(cls,"getDefaultToolkit","()Ljava/awt/Toolkit;");
    AwtToolkit::getFontMetricsMID =
	env->GetMethodID(cls, "getFontMetrics", 
			 "(Ljava/awt/Font;)Ljava/awt/FontMetrics;");
	AwtToolkit::insetsMID =
		env->GetMethodID(env->FindClass("java/awt/Insets"), "<init>", "(IIII)V");

    DASSERT(AwtToolkit::getDefaultToolkitMID != NULL);
    DASSERT(AwtToolkit::getFontMetricsMID != NULL);
	DASSERT(AwtToolkit::insetsMID != NULL);

    CATCH_BAD_ALLOC;
}


} /* extern "C" */

/************************************************************************
 * SunToolkit native methods
 */

extern "C" {
  
/*
 * Class:     sun_awt_SunToolkit
 * Method:    getPrivateKey
 * Signature: (Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_sun_awt_SunToolkit_getPrivateKey(JNIEnv *env, jclass cls, jobject obj)
{
    jobject key = obj;

    /*
     * Fix for BugTraq ID 4254701.
     * Don't use Components and MenuComponents as keys in hash maps.
     * We use private keys instead.
     */
    if (env->IsInstanceOf(obj, getComponentClass(env))) {
        key = env->GetObjectField(obj, AwtComponent::privateKeyID);
    } else if (env->IsInstanceOf(obj, getMenuComponentClass(env))) {
        key = env->GetObjectField(obj, AwtMenuItem::privateKeyID);
    }
    return key;
}

/*
 * Class:     sun_awt_SunToolkit
 * Method:    wakeupEventQueue
 * Signature: (Ljava/awt/EventQueue;Z)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_SunToolkit_wakeupEventQueue(JNIEnv *env, jclass cls, jobject eq, jboolean b)
{

    // get global reference of java/awt/EventQueue class and its wakeup method
    // (run only once)
    static jclass eventQueueCls = NULL;
    static jmethodID wakeupMethodID = NULL;
    if (eventQueueCls == NULL) {
        jclass eventQueueClsLocal = env->FindClass("java/awt/EventQueue");
        DASSERT(eventQueueClsLocal != NULL);
        if (eventQueueClsLocal == NULL) {
            /* exception already thrown */
            return;
        }
        eventQueueCls = (jclass)env->NewGlobalRef(eventQueueClsLocal);
        env->DeleteLocalRef(eventQueueClsLocal);

        wakeupMethodID = env->GetMethodID(eventQueueCls,
                                          "wakeup", "(Z)V");
        DASSERT(wakeupMethodID != NULL);
        if (wakeupMethodID == NULL) {
            return;
        }
    }

    DASSERT(env->IsInstanceOf(eq, eventQueueCls));
    env->CallVoidMethod(eq, wakeupMethodID, b);
}

/*
 * Class:     sun_awt_SunToolkit
 * Method:    setZOrder
 * Signature: (Ljava/awt/Container;Ljava/awt/Component;I)V
 */
JNIEXPORT void JNICALL 
Java_sun_awt_SunToolkit_setZOrder(JNIEnv *env, jclass cls, 
                                  jobject cont, jobject comp, jint index)
{
    TRY;

    if (JNU_IsNull(env, cont)) {
        return;
    }

    static jmethodID setZOrderMID = NULL;

    if ( JNU_IsNull(env, setZOrderMID) ) {
        jclass containerCls = env->FindClass("java/awt/Container");
        DASSERT(!JNU_IsNull(env, containerCls));
        if ( JNU_IsNull(env, containerCls) ) {
            return;
        }
        setZOrderMID = env->GetMethodID(containerCls, "setZOrder",
                                        "(Ljava/awt/Component;I)V");
        DASSERT(!JNU_IsNull(env, setZOrderMID));
        if ( JNU_IsNull(env, setZOrderMID) ) {
            return;
        }
    }

    env->CallVoidMethod(cont, setZOrderMID, comp, index);

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_SunToolkit
 * Method:    getAppContext
 * Signature: (Ljava/awt/Object;)Lsun/awt/AppContext;
 */
JNIEXPORT jobject JNICALL
Java_sun_awt_SunToolkit_getAppContext(JNIEnv *env, jclass cls, jobject obj)
{
    jobject appContext = NULL;

    if (env->IsInstanceOf(obj, getComponentClass(env))) {
	appContext = env->GetObjectField(obj, AwtComponent::appContextID);
    } else if (env->IsInstanceOf(obj, getMenuComponentClass(env))) {
	appContext = env->GetObjectField(obj, AwtMenuItem::appContextID);
    }
    return appContext;
}

/*
 * Class:     sun_awt_SunToolkit
 * Method:    setAppContext
 * Signature: (Ljava/lang/Object;Lsun/awt/AppContext;)Z
 */
JNIEXPORT jboolean JNICALL
Java_sun_awt_SunToolkit_setAppContext(JNIEnv *env, jclass cls, jobject comp,
				      jobject appContext)
{
    jboolean isComponent;
    if (env->IsInstanceOf(comp, getComponentClass(env))) {
	env->SetObjectField(comp, AwtComponent::appContextID, appContext);
	isComponent = JNI_TRUE;
    } else if (env->IsInstanceOf(comp, getMenuComponentClass(env))) {
	env->SetObjectField(comp, AwtMenuItem::appContextID, appContext);
	isComponent = JNI_TRUE;
    } else {
        isComponent = JNI_FALSE;
    }
    return isComponent;
}
} /* extern "C" */

/************************************************************************
 * WToolkit native methods
 */

extern "C" {

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    initIDs
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_initIDs(JNIEnv *env, jclass cls)
{
    TRY;

    AwtToolkit::windowsSettingChangeMID =
	env->GetMethodID(cls, "windowsSettingChange", "()V");
    DASSERT(AwtToolkit::windowsSettingChangeMID != 0);

    AwtToolkit::displayChangeMID =
    env->GetStaticMethodID(cls, "displayChanged", "()V");
    DASSERT(AwtToolkit::displayChangeMID != 0);

    // Set various global IDs needed by JAWT code.  Note: these
    // variables cannot be set by JAWT code directly due to 
    // different permissions that that code may be run under
    // (bug 4796548).  It would be nice to initialize these
    // variables lazily, but given the minimal number of calls
    // for this, it seems simpler to just do it at startup with
    // negligible penalty.
    jclass sDataClassLocal = env->FindClass("sun/java2d/SurfaceData");
    DASSERT(sDataClassLocal != 0);
    jclass vImgClassLocal = env->FindClass("sun/awt/image/SunVolatileImage");
    DASSERT(vImgClassLocal != 0);
    jclass vSMgrClassLocal =
        env->FindClass("sun/awt/image/VolatileSurfaceManager");
    DASSERT(vSMgrClassLocal != 0);
    jclass componentClassLocal = env->FindClass("java/awt/Component");
    DASSERT(componentClassLocal != 0);
    jclass w32ossdClassLocal = 
	env->FindClass("sun/awt/windows/Win32OffScreenSurfaceData");
    DASSERT(w32ossdClassLocal != 0);
    jawtSMgrID = env->GetFieldID(vImgClassLocal, "surfaceManager", 
                                 "Lsun/awt/image/VolatileSurfaceManager;");
    DASSERT(jawtSMgrID != 0);
    jawtSDataID = env->GetFieldID(vSMgrClassLocal, "sdCurrent",
                                  "Lsun/java2d/SurfaceData;");
    DASSERT(jawtSDataID != 0);
    jawtPDataID = env->GetFieldID(sDataClassLocal, "pData", "J");
    DASSERT(jawtPDataID != 0);
    
    // Save these classes in global references for later use
    jawtVImgClass = (jclass)env->NewGlobalRef(vImgClassLocal);
    jawtComponentClass = (jclass)env->NewGlobalRef(componentClassLocal);
    jawtW32ossdClass = (jclass)env->NewGlobalRef(w32ossdClassLocal);

    CATCH_BAD_ALLOC;
}


/*
 * Class:     sun_awt_windows_Toolkit
 * Method:    disableCustomPalette
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_sun_awt_windows_WToolkit_disableCustomPalette(JNIEnv *env, jclass cls) {
    AwtPalette::DisableCustomPalette();
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    embeddedInit
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_sun_awt_windows_WToolkit_embeddedInit(JNIEnv *env, jclass cls)
{
    TRY;

    AwtToolkit::SetEnv(env);

    return AwtToolkit::GetInstance().Initialize(FALSE);

    CATCH_BAD_ALLOC_RET(JNI_FALSE);
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    embeddedDispose
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_sun_awt_windows_WToolkit_embeddedDispose(JNIEnv *env, jclass cls)
{
    TRY;

    BOOL retval = AwtToolkit::GetInstance().Dispose();
    AwtToolkit::GetInstance().SetPeer(env, NULL);
    return retval;

    CATCH_BAD_ALLOC_RET(JNI_FALSE);
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    embeddedEventLoopIdleProcessing
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_embeddedEventLoopIdleProcessing(JNIEnv *env,
    jobject self)
{
    VerifyWindowMoveLockReleased();
}


/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    init
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_sun_awt_windows_WToolkit_init(JNIEnv *env, jobject self)
{
    TRY;

    AwtToolkit::SetEnv(env);
    
    AwtToolkit::GetInstance().SetPeer(env, self);

    // This call will fail if the Toolkit was already initialized.
    // In that case, we don't want to start another message pump.
    return AwtToolkit::GetInstance().Initialize(TRUE);

    CATCH_BAD_ALLOC_RET(FALSE);
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    eventLoop
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_eventLoop(JNIEnv *env, jobject self)
{
    TRY;

    DASSERT(AwtToolkit::GetInstance().localPump());

    AwtToolkit::SetBusy(TRUE);

    AwtToolkit::GetInstance().MessageLoop(AwtToolkit::PrimaryIdleFunc, 
					  AwtToolkit::CommonPeekMessageFunc);

    AwtToolkit::GetInstance().Dispose();

    AwtToolkit::SetBusy(FALSE);

    /*
     * IMPORTANT NOTES:
     *   The AwtToolkit has been destructed by now.
     * DO NOT CALL any method of AwtToolkit!!!
     */

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    shutdown
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_shutdown(JNIEnv *env, jobject self)
{
    TRY;

    AwtToolkit& tk = AwtToolkit::GetInstance();

    tk.QuitMessageLoop(AwtToolkit::EXIT_ALL_ENCLOSING_LOOPS);
    
    while (!tk.IsDisposed()) {
        Sleep(100);
    }

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    startSecondaryEventLoop
 * Signature: ()V;
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_startSecondaryEventLoop(
    JNIEnv *env,
    jclass)
{
    TRY;

    DASSERT(AwtToolkit::MainThread() == ::GetCurrentThreadId());

    AwtToolkit::GetInstance().MessageLoop(AwtToolkit::SecondaryIdleFunc,
					  AwtToolkit::CommonPeekMessageFunc);

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    quitSecondaryEventLoop
 * Signature: ()V;
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_quitSecondaryEventLoop(
    JNIEnv *env,
    jclass)
{
    TRY;

    AwtToolkit::GetInstance().QuitMessageLoop(AwtToolkit::EXIT_ENCLOSING_LOOP);

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    makeColorModel
 * Signature: ()Ljava/awt/image/ColorModel;
 */
JNIEXPORT jobject JNICALL 
Java_sun_awt_windows_WToolkit_makeColorModel(JNIEnv *env, jclass cls) 
{
    TRY;

    return AwtWin32GraphicsDevice::GetColorModel(env, JNI_FALSE,
	AwtWin32GraphicsDevice::GetDefaultDeviceIndex());

    CATCH_BAD_ALLOC_RET(NULL);
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    getScreenResolution
 * Signature: ()I
 */
JNIEXPORT jint JNICALL 
Java_sun_awt_windows_WToolkit_getScreenResolution(JNIEnv *env, jobject self) 
{
    TRY;

    HWND hWnd = ::GetDesktopWindow();
    HDC hDC = ::GetDC(hWnd);
    jint result = ::GetDeviceCaps(hDC, LOGPIXELSX);
    ::ReleaseDC(hWnd, hDC);
    return result;

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    getMaximumCursorColors
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WToolkit_getMaximumCursorColors(JNIEnv *env, jobject self)
{
    TRY;

    HDC hIC = ::CreateIC(TEXT("DISPLAY"), NULL, NULL, NULL);

    int nColor = 256;
    switch (::GetDeviceCaps(hIC, BITSPIXEL) * ::GetDeviceCaps(hIC, PLANES)) {
	case 1:		nColor = 2; 		break;
	case 4:		nColor = 16; 		break;
	case 8:		nColor = 256; 		break;
        case 16:        nColor = 65536;         break;
	case 24:	nColor = 16777216; 	break;
    }
    ::DeleteDC(hIC);
    return nColor;

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    getScreenWidth
 * Signature: ()I
 */
JNIEXPORT jint JNICALL 
Java_sun_awt_windows_WToolkit_getScreenWidth(JNIEnv *env, jobject self)
{
    TRY;

    return ::GetSystemMetrics(SM_CXSCREEN);

    CATCH_BAD_ALLOC_RET(0);
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    getScreenHeight
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_windows_WToolkit_getScreenHeight(JNIEnv *env, jobject self)
{
    TRY;

    return ::GetSystemMetrics(SM_CYSCREEN);

    CATCH_BAD_ALLOC_RET(0);
}


/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    getSreenInsets
 * Signature: (I)Ljava/awt/Insets;
 */
JNIEXPORT jobject JNICALL 
Java_sun_awt_windows_WToolkit_getScreenInsets(JNIEnv *env,
                                              jobject self,
                                              jint screen)
{
    jobject insets = NULL;
    RECT rRW;
    MONITOR_INFO *miInfo;     

    TRY;

/* if primary display */
   if (screen == 0) {
      if (::SystemParametersInfo(SPI_GETWORKAREA,0,(void *) &rRW,0) == TRUE) {
          insets = env->NewObject(env->FindClass("java/awt/Insets"),
             AwtToolkit::insetsMID,
             rRW.top,
             rRW.left,
             ::GetSystemMetrics(SM_CYSCREEN) - rRW.bottom,
             ::GetSystemMetrics(SM_CXSCREEN) - rRW.right);
      }
    }
    
/* if additional display */    
    else {
	miInfo = AwtWin32GraphicsDevice::GetMonitorInfo(screen);
        if (miInfo) {
            insets = env->NewObject(env->FindClass("java/awt/Insets"),
                AwtToolkit::insetsMID,
                miInfo->rWork.top    - miInfo->rMonitor.top,
                miInfo->rWork.left   - miInfo->rMonitor.left,
                miInfo->rMonitor.bottom - miInfo->rWork.bottom,
                miInfo->rMonitor.right - miInfo->rWork.right);
        }
    }

    if (safe_ExceptionOccurred(env)) {
        return 0;
    }
    return insets;
	
    CATCH_BAD_ALLOC_RET(NULL);
}


/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    sync
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_sync(JNIEnv *env, jobject self)
{
    TRY;

    // Synchronize both GDI and DDraw
    VERIFY(::GdiFlush());
    DDSync();

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    beep
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_beep(JNIEnv *env, jobject self)
{
    TRY;

    VERIFY(::MessageBeep(MB_OK));

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    getLockingKeyStateNative
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_sun_awt_windows_WToolkit_getLockingKeyStateNative(JNIEnv *env, jobject self, jint javaKey)
{
    TRY;

    UINT windowsKey, modifiers;
    AwtComponent::JavaKeyToWindowsKey(javaKey, &windowsKey, &modifiers);
    
    if (windowsKey == 0) {
        JNU_ThrowByName(env, "java/lang/UnsupportedOperationException", "Keyboard doesn't have requested key");
        return JNI_FALSE;
    }
    
    // low order bit in keyboardState indicates whether the key is toggled 
    BYTE keyboardState[AwtToolkit::KB_STATE_SIZE];
    AwtToolkit::GetKeyboardState(keyboardState);
    return keyboardState[windowsKey] & 0x01;

    CATCH_BAD_ALLOC_RET(JNI_FALSE);
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    setLockingKeyStateNative
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_setLockingKeyStateNative(JNIEnv *env, jobject self, jint javaKey, jboolean state)
{
    TRY;

    UINT windowsKey, modifiers;
    AwtComponent::JavaKeyToWindowsKey(javaKey, &windowsKey, &modifiers);
    
    if (windowsKey == 0) {
        JNU_ThrowByName(env, "java/lang/UnsupportedOperationException", "Keyboard doesn't have requested key");
        return;
    }

    // if the key isn't in the desired state yet, simulate key events to get there
    // low order bit in keyboardState indicates whether the key is toggled 
    BYTE keyboardState[AwtToolkit::KB_STATE_SIZE];
    AwtToolkit::GetKeyboardState(keyboardState);
    if ((keyboardState[windowsKey] & 0x01) != state) {
        ::keybd_event(windowsKey, 0, 0, 0);
        ::keybd_event(windowsKey, 0, KEYEVENTF_KEYUP, 0);
    }

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    loadSystemColors
 * Signature: ([I)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_loadSystemColors(JNIEnv *env, jobject self,
					       jintArray colors)
{
    TRY;

    static int indexMap[] = {
        COLOR_DESKTOP, /* DESKTOP */
        COLOR_ACTIVECAPTION, /* ACTIVE_CAPTION */
        COLOR_CAPTIONTEXT, /* ACTIVE_CAPTION_TEXT */
        COLOR_ACTIVEBORDER, /* ACTIVE_CAPTION_BORDER */
        COLOR_INACTIVECAPTION, /* INACTIVE_CAPTION */
        COLOR_INACTIVECAPTIONTEXT, /* INACTIVE_CAPTION_TEXT */
        COLOR_INACTIVEBORDER, /* INACTIVE_CAPTION_BORDER */
        COLOR_WINDOW, /* WINDOW */
        COLOR_WINDOWFRAME, /* WINDOW_BORDER */
        COLOR_WINDOWTEXT, /* WINDOW_TEXT */
        COLOR_MENU, /* MENU */
        COLOR_MENUTEXT, /* MENU_TEXT */
        COLOR_WINDOW, /* TEXT */
        COLOR_WINDOWTEXT, /* TEXT_TEXT */
        COLOR_HIGHLIGHT, /* TEXT_HIGHLIGHT */
        COLOR_HIGHLIGHTTEXT, /* TEXT_HIGHLIGHT_TEXT */
        COLOR_GRAYTEXT, /* TEXT_INACTIVE_TEXT */
        COLOR_3DFACE, /* CONTROL */
        COLOR_BTNTEXT, /* CONTROL_TEXT */
        COLOR_3DLIGHT, /* CONTROL_HIGHLIGHT */
        COLOR_3DHILIGHT, /* CONTROL_LT_HIGHLIGHT */
        COLOR_3DSHADOW, /* CONTROL_SHADOW */
        COLOR_3DDKSHADOW, /* CONTROL_DK_SHADOW */
        COLOR_SCROLLBAR, /* SCROLLBAR */
        COLOR_INFOBK, /* INFO */
        COLOR_INFOTEXT, /* INFO_TEXT */
    };

    jint colorLen = env->GetArrayLength(colors);
    jint* colorsPtr = NULL;
    try {
        colorsPtr = (jint *)env->GetPrimitiveArrayCritical(colors, 0);
	for (int i = 0; i < sizeof indexMap && i < colorLen; i++) {
	    colorsPtr[i] = DesktopColor2RGB(indexMap[i]);
	}
    } catch (...) {
        if (colorsPtr != NULL) {
	    env->ReleasePrimitiveArrayCritical(colors, colorsPtr, 0);
	}
	throw;
    }

    env->ReleasePrimitiveArrayCritical(colors, colorsPtr, 0);

    CATCH_BAD_ALLOC;
}

extern "C" JNIEXPORT jobject JNICALL DSGetComponent
    (JNIEnv* env, void* platformInfo)
{
    TRY;

    HWND hWnd = (HWND)platformInfo;
    if (!::IsWindow(hWnd))
        return NULL;

    AwtComponent* comp = AwtComponent::GetComponent(hWnd);
    if (comp == NULL)
        return NULL;

    return comp->GetTarget(env);

    CATCH_BAD_ALLOC_RET(NULL);
}

JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_pushModality(JNIEnv *env, jobject self)
{
    TRY;

    JNI_CHECK_NULL_RETURN(self, "null peer");
    AwtDialog::ModalDisable(NULL);

    CATCH_BAD_ALLOC;
}

JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_popModality(JNIEnv *env, jobject self)
{
    TRY;

    JNI_CHECK_NULL_RETURN(self, "null peer");
    AwtDialog::ModalEnable(NULL);
    // shouldn't be necessary since containing app will
    // reenable it's parent window when it's dialog goes
    // away
//    AwtDialog::ModalNextWindowToFront(GetHWnd());

    CATCH_BAD_ALLOC;
}

JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_finalize(JNIEnv *env, jobject self)
{
#ifdef DEBUG
    TRY_NO_VERIFY;

    // If this method was called, that means runFinalizersOnExit is turned
    // on and the VM is exiting cleanly. We should signal the debug memory
    // manager to generate a leaks report.
    AwtDebugSupport::GenerateLeaksReport();

    CATCH_BAD_ALLOC;
#endif
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    setDynamicLayoutNative
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_setDynamicLayoutNative(JNIEnv *env, 
  jobject self, jboolean dynamic)
{
    TRY;

    AwtToolkit::GetInstance().SetDynamicLayout(dynamic);

    CATCH_BAD_ALLOC;
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    isDynamicLayoutSupportedNative
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_sun_awt_windows_WToolkit_isDynamicLayoutSupportedNative(JNIEnv *env,
  jobject self)
{
    TRY;

    return (jboolean) AwtToolkit::GetInstance().IsDynamicLayoutSupported();

    CATCH_BAD_ALLOC_RET(FALSE);
}

/*
 * Class:     sun_awt_SunToolkit
 * Method:    setLWRequestStatus
 * Signature: (Ljava/lang/Object;Z)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_SunToolkit_setLWRequestStatus(JNIEnv *env, jclass cls, jobject win,
                                         jboolean status)
{
    static jclass windowCls = NULL;
    static jfieldID lwRequestStatus;

    if (windowCls == NULL) {
        jclass windowClsLocal = env->FindClass("java/awt/Window");
        DASSERT(windowClsLocal != NULL);
        if (windowClsLocal == NULL) {
            return;
        }
        windowCls = (jclass)env->NewGlobalRef(windowClsLocal);
        env->DeleteLocalRef(windowClsLocal);
        lwRequestStatus = env->GetFieldID(windowCls, "syncLWRequests", "Z");
    }
    env->SetBooleanField(win, lwRequestStatus, status);
}

/*
 * Class:     sun_awt_windows_WToolkit
 * Method:    printWindowsVersion
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_windows_WToolkit_printWindowsVersion(JNIEnv *env, jclass cls) 
{
    TRY;

    AwtToolkit::PrintWinVersion();

    CATCH_BAD_ALLOC;
}

} /* extern "C" */

/* Convert a Windows desktop color index into an RGB value. */
COLORREF DesktopColor2RGB(int colorIndex) {
    DWORD sysColor = ::GetSysColor(colorIndex);
    return ((GetRValue(sysColor)<<16) | (GetGValue(sysColor)<<8) | 
	    (GetBValue(sysColor)) | 0xff000000);
}

