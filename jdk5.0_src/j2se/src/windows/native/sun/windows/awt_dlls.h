/*
 * @(#)awt_dlls.h	1.19 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#ifndef AWT_DLLS_H
#define AWT_DLLS_H

#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include "awt_FileDialog.h"
#include "awt_PrintDialog.h"

/*
 * To reduce memory footprint we don't statically link to COMDLG32.DLL
 * and SHELL32.  Instead we programatically load them only when they are
 * needed.
 */

//---------------------------------------------------------------------------

typedef BOOL (APIENTRY *PrintDlgType)(LPPRINTDLGW);
typedef BOOL (APIENTRY *PageSetupDlgType)(LPPAGESETUPDLGW);
typedef BOOL (APIENTRY *GetOpenFileNameType)(LPOPENFILENAMEW);
typedef BOOL (APIENTRY *GetSaveFileNameType)(LPOPENFILENAMEW);
typedef DWORD (APIENTRY *GetExtendedErrorType)(VOID);

class AwtCommDialog {
public:
    static DWORD CommDlgExtendedError(VOID);

    static BOOL PrintDlg(LPPRINTDLG data);

    static BOOL PageSetupDlg(LPPAGESETUPDLG data);

private:
    static void load_comdlg_procs();

    // Use wrapper functions with default calling convention. If the
    // default isn't __stdcall, accessing the Win32 functions directly
    // will cause stack corruption if we cast away __stdcall.
    static BOOL PrintDlgWrapper(LPPRINTDLG data) {
        return (*do_print_dlg)(data);
    }
    static BOOL PageSetupDlgWrapper(LPPAGESETUPDLG data) {
        return (*do_page_setup_dlg)(data);
    }
    static BOOL GetOpenFileNameWrapper(LPOPENFILENAME data) {
        return (*get_open_file_name)(data);
    }
    static BOOL GetSaveFileNameWrapper(LPOPENFILENAME data) {
        return (*get_save_file_name)(data);
    }
    static DWORD GetExtendedErrorWrapper(VOID) {
        return (*get_dlg_extended_error)();
    }

    friend BOOL AwtFileDialog::GetOpenFileName(LPAWTOPENFILENAME);
    friend BOOL AwtFileDialog::GetSaveFileName(LPAWTOPENFILENAME);
    friend BOOL AwtPrintDialog::PrintDlg(LPPRINTDLG);

    static PrintDlgType do_print_dlg;
    static PageSetupDlgType do_page_setup_dlg;
    static GetOpenFileNameType get_open_file_name;
    static GetSaveFileNameType get_save_file_name;
    static GetExtendedErrorType get_dlg_extended_error;
};

//---------------------------------------------------------------------------

// Dynamically load in SHELL32.DLL and define the procedure pointers listed below.
extern void load_shell_procs();

// Procedure pointers obtained from SHELL32.DLL
// You must call load_shell_procs() before using any of these.
typedef UINT (APIENTRY *DragQueryFileType)(HDROP,UINT,LPTSTR,UINT);
typedef BOOL (APIENTRY *GetPathFromIDListType)(LPCITEMIDLIST,LPTSTR);
extern DragQueryFileType do_drag_query_file;
extern GetPathFromIDListType get_path_from_idlist;

//---------------------------------------------------------------------------

// Dynamically load in USER32.DLL and define the procedure pointers listed below.
extern void load_user_procs();

// Procedure pointers obtained from USER32.DLL
// You must call load_user_procs() before using any of these.
typedef BOOL (WINAPI *AnimateWindowType)(HWND,DWORD,DWORD);
typedef LONG (WINAPI *ChangeDisplaySettingsExType)(LPCTSTR,LPDEVMODE,HWND,DWORD,LPVOID lParam);
extern AnimateWindowType fn_animate_window;
extern ChangeDisplaySettingsExType fn_change_display_settings_ex;

//---------------------------------------------------------------------------

// Dynamically load in VERSION.DLL and define the procedure pointers listed below.
extern void load_version_procs();

// Procedure pointers obtained from VERSION.DLL
// You must call load_version_procs() before using any of these.
typedef DWORD (APIENTRY *GetFileVersionInfoSizeType)(LPTSTR,LPDWORD);
typedef BOOL  (APIENTRY *GetFileVersionInfoType)(LPTSTR,DWORD,DWORD,LPVOID);
typedef BOOL  (APIENTRY *VerQueryValueType)(const LPVOID,LPTSTR,LPVOID*,PUINT);
extern GetFileVersionInfoSizeType get_file_version_info_size;
extern GetFileVersionInfoType get_file_version_info;
extern VerQueryValueType do_ver_query_value;

//---------------------------------------------------------------------------

// Dynamically load in RSRC32.DLL and define the procedure pointers listed below.
extern void load_rsrc32_procs();

// Procedure pointers obtained from RSRC32.DLL
// You must call load_rsrc32_procs() before using this procedure.

/*
 * NOTE: even after load_rsrc32_procs() you must check that 
 * the function pointer is valid before use. 
 * It will be NULL in three cases:
 *  1.RSRC32.DLL not found. This means that Resource Meter
 *    isn't installed. 
 *  2.RSRC32.DLL can't be loaded. This happens on WinNT.
 *  3.Unknown version of RSRC32.DLL. This is undocumented
 *    procedure, so the safest will be to use it only for
 *    a finite set of known versions.
 */
typedef UINT (APIENTRY *GetFreeSystemResourcesType)(UINT);

extern GetFreeSystemResourcesType get_free_system_resources;

extern void load_rich_edit_library();

//---------------------------------------------------------------------------

/*
 * Loading WINMM.DLL (the Windows MultiMedia library) is extremely
 * expensive. The AWT only uses it to play certain Windows sounds
 * (which are off by default) so we dynamically load it upon demand
 * instead of statically linking to it.
 */

class AwtWinMM {
public:
    static BOOL PlaySoundWrapper(LPCTSTR pszSound, HMODULE hmod, DWORD fdwSound);

private:
    static void load_winmm_procs();
    static bool initialized;
    typedef BOOL WINAPI PlaySoundWFunc(LPCTSTR pszSound, HMODULE hmod, DWORD fdwSound);
    static PlaySoundWFunc* playSoundFunc;
};

#endif /* AWT_DLLS_H */
