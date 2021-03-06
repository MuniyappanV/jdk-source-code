/* $XConsortium: Manager.h /main/5 1995/07/15 20:52:43 drk $ */
/*
 *  @OSF_COPYRIGHT@
 *  COPYRIGHT NOTICE
 *  Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 *  ALL RIGHTS RESERVED (MOTIF). See the file named COPYRIGHT.MOTIF for
 *  the full copyright text.
 */
/*
 * HISTORY
 */
#ifndef _XmManager_h
#define _XmManager_h

#include <Xm/Xm.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XmIsManager
#define XmIsManager(w) XtIsSubclass(w, xmManagerWidgetClass)
#endif /* XmIsManager */

externalref WidgetClass xmManagerWidgetClass;

typedef struct _XmManagerClassRec * XmManagerWidgetClass;
typedef struct _XmManagerRec      * XmManagerWidget;


/********    Public Function Declarations    ********/


/********    End Public Function Declarations    ********/


#ifdef __cplusplus
}  /* Close scope of 'extern "C"' declaration which encloses file. */
#endif

#endif /* _XmManager_h */
/* DON'T ADD ANYTHING AFTER THIS #endif */
