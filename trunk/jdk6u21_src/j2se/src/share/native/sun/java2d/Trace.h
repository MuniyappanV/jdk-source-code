/*
 * @(#)Trace.h	1.9 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#ifndef _Included_Trace
#define _Included_Trace

#include <jni.h>
#include "debug_trace.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * J2dTrace
 * Trace utility used throughout Java 2D code.  Uses a "level"
 * parameter that allows user to specify how much detail
 * they want traced at runtime.  Tracing is only enabled
 * in debug mode, to avoid overhead running release build.
 */

#define J2D_TRACE_INVALID 	-1
#define J2D_TRACE_OFF 		0
#define J2D_TRACE_ERROR 	1
#define J2D_TRACE_WARNING 	2
#define J2D_TRACE_INFO 		3
#define J2D_TRACE_VERBOSE 	4
#define J2D_TRACE_VERBOSE2 	5
#define J2D_TRACE_MAX 	        (J2D_TRACE_VERBOSE2+1)

JNIEXPORT void JNICALL
J2dTraceImpl(int level, jboolean cr, const char *string, ...);
JNIEXPORT void JNICALL
J2dTraceInit();

#ifndef DEBUG
#define J2dTrace(level, string)
#define J2dTrace1(level, string, arg1)
#define J2dTrace2(level, string, arg1, arg2)
#define J2dTrace3(level, string, arg1, arg2, arg3)
#define J2dTrace4(level, string, arg1, arg2, arg3, arg4)
#define J2dTrace5(level, string, arg1, arg2, arg3, arg4, arg5)
#define J2dTrace6(level, string, arg1, arg2, arg3, arg4, arg5, arg6)
#define J2dTrace7(level, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define J2dTrace8(level, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define J2dTraceLn(level, string)
#define J2dTraceLn1(level, string, arg1)
#define J2dTraceLn2(level, string, arg1, arg2)
#define J2dTraceLn3(level, string, arg1, arg2, arg3)
#define J2dTraceLn4(level, string, arg1, arg2, arg3, arg4)
#define J2dTraceLn5(level, string, arg1, arg2, arg3, arg4, arg5)
#define J2dTraceLn6(level, string, arg1, arg2, arg3, arg4, arg5, arg6)
#define J2dTraceLn7(level, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define J2dTraceLn8(level, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#else /* DEBUG */
#define J2dTrace(level, string) { \
	    J2dTraceImpl(level, JNI_FALSE, string); \
	}
#define J2dTrace1(level, string, arg1) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1); \
	}
#define J2dTrace2(level, string, arg1, arg2) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2); \
	}
#define J2dTrace3(level, string, arg1, arg2, arg3) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2, arg3); \
	}
#define J2dTrace4(level, string, arg1, arg2, arg3, arg4) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2, arg3, arg4); \
	}
#define J2dTrace5(level, string, arg1, arg2, arg3, arg4, arg5) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2, arg3, arg4, arg5); \
	}
#define J2dTrace6(level, string, arg1, arg2, arg3, arg4, arg5, arg6) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2, arg3, arg4, arg5, arg6); \
	}
#define J2dTrace7(level, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7); \
	}
#define J2dTrace8(level, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); \
	}
#define J2dTraceLn(level, string) { \
	    J2dTraceImpl(level, JNI_TRUE, string); \
	}
#define J2dTraceLn1(level, string, arg1) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1); \
	}
#define J2dTraceLn2(level, string, arg1, arg2) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2); \
	}
#define J2dTraceLn3(level, string, arg1, arg2, arg3) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2, arg3); \
	}
#define J2dTraceLn4(level, string, arg1, arg2, arg3, arg4) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2, arg3, arg4); \
	}
#define J2dTraceLn5(level, string, arg1, arg2, arg3, arg4, arg5) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2, arg3, arg4, arg5); \
	}
#define J2dTraceLn6(level, string, arg1, arg2, arg3, arg4, arg5, arg6) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2, arg3, arg4, arg5, arg6); \
	}
#define J2dTraceLn7(level, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7); \
	}
#define J2dTraceLn8(level, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); \
	}
#endif /* DEBUG */


/**
 * NOTE: Use the following RlsTrace calls very carefully; they are compiled
 * into the code and should thus not be put in any performance-sensitive
 * areas.
 */

#define J2dRlsTrace(level, string) { \
	    J2dTraceImpl(level, JNI_FALSE, string); \
	}
#define J2dRlsTrace1(level, string, arg1) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1); \
	}
#define J2dRlsTrace2(level, string, arg1, arg2) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2); \
	}
#define J2dRlsTrace3(level, string, arg1, arg2, arg3) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2, arg3); \
	}
#define J2dRlsTrace4(level, string, arg1, arg2, arg3, arg4) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2, arg3, arg4); \
	}
#define J2dRlsTrace5(level, string, arg1, arg2, arg3, arg4, arg5) { \
	    J2dTraceImpl(level, JNI_FALSE, string, arg1, arg2, arg3, arg4, arg5); \
	}
#define J2dRlsTraceLn(level, string) { \
	    J2dTraceImpl(level, JNI_TRUE, string); \
	}
#define J2dRlsTraceLn1(level, string, arg1) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1); \
	}
#define J2dRlsTraceLn2(level, string, arg1, arg2) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2); \
	}
#define J2dRlsTraceLn3(level, string, arg1, arg2, arg3) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2, arg3); \
	}
#define J2dRlsTraceLn4(level, string, arg1, arg2, arg3, arg4) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2, arg3, arg4); \
	}
#define J2dRlsTraceLn5(level, string, arg1, arg2, arg3, arg4, arg5) { \
	    J2dTraceImpl(level, JNI_TRUE, string, arg1, arg2, arg3, arg4, arg5); \
	}

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* _Included_Trace */
