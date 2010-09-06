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
#ifdef REV_INFO
#ifndef lint
static char rcsid[] = "$XConsortium: DialogSE.c /main/10 1995/07/14 10:19:03 drk $"
#endif
#endif
/*
*  (c) Copyright 1989, DIGITAL EQUIPMENT CORPORATION, MAYNARD, MASS. */
/*
*  (c) Copyright 1987, 1988, 1989, 1990, 1991, 1992 HEWLETT-PACKARD COMPANY */
/*
*  (c) Copyright 1988 MASSACHUSETTS INSTITUTE OF TECHNOLOGY  */

#include <Xm/Xm.h>		/* To make cpp on Sun happy. CR 5943 */
#include <Xm/DialogSEP.h>
#include <Xm/BaseClassP.h>
#include <X11/ShellP.h>


/********    Static Function Declarations    ********/

static void ClassInitialize( void ) ;
static void DeleteWindowHandler( 
                        Widget wid,
                        XtPointer closure,
                        XtPointer call_data) ;
static Widget GetManagedKid( 
                        CompositeWidget p) ;

/********    End Static Function Declarations    ********/


static XtResource extResources[]= {	
    {
	XmNdeleteResponse, XmCDeleteResponse, 
	XmRDeleteResponse, sizeof(unsigned char),
	XtOffsetOf( struct _XmDialogShellExtRec, vendor.delete_response), 
	XmRImmediate, (XtPointer) XmUNMAP,
    },
};


static XmBaseClassExtRec       myExtExtension = {
    NULL,                                     /* Next extension       */
    NULLQUARK,                                /* record type XmQmotif */
    XmBaseClassExtVersion,                    /* version              */
    sizeof(XmBaseClassExtRec),                /* size                 */
    XmInheritInitializePrehook,	              /* initialize prehook   */
    XmInheritSetValuesPrehook,	              /* set_values prehook   */
    XmInheritInitializePosthook,              /* initialize posthook  */
    XmInheritSetValuesPosthook,               /* set_values posthook  */
    XmInheritClass,		              /* secondary class      */
    XmInheritSecObjectCreate,	              /* creation proc        */
    XmInheritGetSecResData,                   /* getSecRes data       */
    {0},                                   /* fast subclass        */
    XmInheritGetValuesPrehook,	              /* get_values prehook   */
    XmInheritGetValuesPosthook,	              /* get_values posthook  */
    XmInheritClassPartInitPrehook,	      /* class_part_prehook   */
    XmInheritClassPartInitPosthook,	      /* class_part_posthook  */
    NULL,	 			      /* compiled_ext_resources*/   
    NULL,	 			      /* ext_resources       	*/   
    0,					      /* resource_count     	*/   
    TRUE,				      /* use_sub_resources	*/
};

/* ext rec static initialization */
externaldef(xmdialogshellextclassrec)
XmDialogShellExtClassRec xmDialogShellExtClassRec = {
    {	
	(WidgetClass) &xmVendorShellExtClassRec, /* superclass	*/   
	"XmDialogShell",		/* class_name 		*/   
	sizeof(XmDialogShellExtRec), 	/* size 		*/   
	ClassInitialize, 		/* Class Initializer 	*/   
	NULL,		 		/* class_part_init 	*/ 
	FALSE, 				/* Class init'ed ? 	*/   
	NULL,	 			/* initialize         	*/   
	NULL, 				/* initialize_notify    */ 
	NULL,	 			/* realize            	*/   
	NULL,	 			/* actions            	*/   
	0,				/* num_actions        	*/   
	extResources, 			/* resources          	*/   
	XtNumber(extResources),		/* resource_count     	*/   
	NULLQUARK, 			/* xrm_class          	*/   
	FALSE, 				/* compress_motion    	*/   
	FALSE, 				/* compress_exposure  	*/   
	FALSE, 				/* compress_enterleave	*/   
	FALSE, 				/* visible_interest   	*/   
	NULL,				/* destroy            	*/   
	NULL,           		/* resize             	*/   
	NULL, 				/* expose             	*/   
	NULL,		 		/* set_values         	*/   
	NULL, 				/* set_values_hook      */ 
	NULL,			 	/* set_values_almost    */ 
	NULL,				/* get_values_hook      */ 
	NULL, 				/* accept_focus       	*/   
	XtVersion, 			/* intrinsics version 	*/   
	NULL, 				/* callback offsets   	*/   
	NULL,				/* tm_table           	*/   
	NULL, 				/* query_geometry       */ 
	NULL, 				/* display_accelerator  */ 
	(XtPointer) &myExtExtension,	/* extension            */ 
    },	
    {
	NULL,				/* synthetic resources	*/
	0,				/* num syn resources	*/
	NULL,				/* extension		*/
    },
    {					/* desktop		*/
	NULL,				/* child_class		*/
	XtInheritInsertChild,		/* insert_child		*/
	XtInheritDeleteChild,		/* delete_child		*/
	NULL,				/* extension		*/
    },
    {					/* shell extension	*/
	XmInheritEventHandler,		/* structureNotify	*/
	NULL,				/* extension		*/
    },
    {					/* vendor ext		*/
	DeleteWindowHandler,            /* delete window handler*/
	XmInheritProtocolHandler,	/* offset_handler	*/
	NULL,				/* extension		*/
    },
    {					/* dialog ext		*/
	(XtPointer) NULL,		/* extension		*/
    }
};

externaldef(xmdialogshellextobjectclass) 
    WidgetClass xmDialogShellExtObjectClass = (WidgetClass) &xmDialogShellExtClassRec;


static void 
ClassInitialize( void )
{
    myExtExtension.record_type = XmQmotif;
}

/************************************************************************
 *
 *  DeleteWindowHandler
 *
 ************************************************************************/
/*ARGSUSED*/
static void 
DeleteWindowHandler(
        Widget wid,
        XtPointer closure,
        XtPointer call_data )	/* unused */
{
    VendorShellWidget	w = (VendorShellWidget) wid ;
    XmVendorShellExtObject ve = (XmVendorShellExtObject) closure;

    switch(ve->vendor.delete_response)
      {
	case XmUNMAP:
	  {
	      Widget managedKid;
	      
	      if ((managedKid = GetManagedKid((CompositeWidget) w)) != NULL)
		XtUnmanageChild(managedKid);
	      break;
	  }
	case XmDESTROY:
	  XtDestroyWidget(wid);
	  break;
	  
	case XmDO_NOTHING:
	default:
	  break;
      }
}    

static Widget 
GetManagedKid(
        CompositeWidget p )
{
    Cardinal	i;
    Widget	*currKid;
    
    for (i = 0, currKid = p->composite.children;
	 i < p->composite.num_children;
	 i++, currKid++)
      {
	  if (XtIsManaged(*currKid))
	    return (*currKid);
      }
    return NULL;
}
