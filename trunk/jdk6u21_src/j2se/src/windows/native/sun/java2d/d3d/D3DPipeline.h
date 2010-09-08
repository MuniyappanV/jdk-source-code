#pragma once

#ifdef DEBUG
#define D3D_DEBUG_INFO
#endif // DEBUG

#ifdef D3D_PPL_DLL


    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif

    #ifdef D3DPIPELINE_EXPORTS
    #define D3DPIPELINE_API __declspec(dllexport)
    #else
    #define D3DPIPELINE_API __declspec(dllimport)
    #endif

    #include <windows.h>
    #include <d3d9.h>
    #include <DDErr.h>
    #include "..\Import\Trace.h"

    #define DebugPrintD3DError(res, msg) \
        DXTRACE_ERR(msg, res)

#else

    #define D3DPIPELINE_API __declspec(dllexport)

    // this include ensures that with debug build we get
    // awt's overridden debug "new" and "delete" operators
    #include "awt.h"

    #include <windows.h>
    #include <d3d9.h>
    #include "Trace.h"

    #define DebugPrintD3DError(res, msg) \
        J2dTraceLn1(J2D_TRACE_ERROR, "D3D Error: " ## msg ## " res=%d", res)

#endif /*D3D_PPL_DLL*/

// some helper macros
#define SAFE_RELEASE(RES) \
do {                      \
    if ((RES)!= NULL) {   \
        (RES)->Release(); \
        (RES) = NULL;     \
    }                     \
} while (0);

#define SAFE_DELETE(RES)  \
do {                      \
    if ((RES)!= NULL) {   \
        delete (RES);     \
        (RES) = NULL;     \
    }                     \
} while (0);

#ifdef DEBUG
#define SAFE_PRINTLN(RES) \
do {                      \
    if ((RES)!= NULL) {   \
        J2dTraceLn1(J2D_TRACE_VERBOSE, "  " ## #RES ## "=0x%x", (RES)); \
    } else {              \
        J2dTraceLn(J2D_TRACE_VERBOSE, "  " ## #RES ## "=NULL"); \
    }                     \
} while (0);
#else // DEBUG
#define SAFE_PRINTLN(RES)
#endif // DEBUG

/*
 * The following macros allow the caller to return (or continue) if the
 * provided value is NULL.  (The strange else clause is included below to
 * allow for a trailing ';' after RETURN/CONTINUE_IF_NULL() invocations.)
 */
#define ACT_IF_NULL(ACTION, value)         \
    if ((value) == NULL) {                 \
        J2dTraceLn3(J2D_TRACE_ERROR,       \
                    "%s is null in %s:%d", #value, __FILE__, __LINE__); \
        ACTION;                            \
    } else do { } while (0)
#define RETURN_IF_NULL(value)   ACT_IF_NULL(return, value)
#define CONTINUE_IF_NULL(value) ACT_IF_NULL(continue, value)
#define RETURN_STATUS_IF_NULL(value, status) \
        ACT_IF_NULL(return (status), value)

#define RETURN_STATUS_IF_EXP_FAILED(EXPR) \
    if (FAILED(res = (EXPR))) {                    \
        DebugPrintD3DError(res, " " ## #EXPR ## " failed in " ## __FILE__); \
        return res;                   \
    } else do { } while (0)

#define RETURN_STATUS_IF_FAILED(status) \
    if (FAILED((status))) {                    \
        DebugPrintD3DError((status), " failed in " ## __FILE__ ## ", return;");\
        return (status);                   \
    } else do { } while (0)
