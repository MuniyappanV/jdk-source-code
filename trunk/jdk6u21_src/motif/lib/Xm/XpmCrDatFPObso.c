/* 
 * @OSF_COPYRIGHT@
 * (c) Copyright 1990, 1991, 1992, 1993, 1994 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 *  
*/ 
/*
 * HISTORY
 * Motif Release 1.2.5
*/
/************************************************************************* 
 **  (c) Copyright 1993, 1994 Hewlett-Packard Company
 **  (c) Copyright 1993, 1994 International Business Machines Corp.
 **  (c) Copyright 2002 Sun Microsystems, Inc.
 **  (c) Copyright 1993, 1994 Novell, Inc.
 *************************************************************************/
#ifdef REV_INFO
#ifndef lint
static char rcsid[] = "$XConsortium: XpmCrDataFP.c /main/cde1_maint/2 1995/08/18 19:38:22 drk $"
#endif
#endif
/* Copyright 1990-92 GROUPE BULL -- See license conditions in file COPYRIGHT */
/*****************************************************************************\
 * XpmCrDataFP.c:                                                            *
 *  XPM library                                                              *
 *  Scan a pixmap and possibly its mask and create an XPM array              *
 *  Developed by Arnaud Le Hors                                              *
 * Added for backward compatibility with 1.2 API			     *
\*****************************************************************************/

#include <Xm/XpmI.h>
#ifdef VMS
#include "sys$library:string.h"
#else
#if defined(SYSV) || defined(SVR4)
#include <string.h>
#else
#include <strings.h>
#endif
#endif

int
_XmXpmCreateDataFromPixmap(display, data_return, pixmap, shapemask, attributes)
    Display *display;
    char ***data_return;
    Pixmap pixmap;
    Pixmap shapemask;
    XpmAttributes *attributes;
{
    XImage *image = NULL;
    XImage *shapeimage = NULL;
    unsigned int width = 0;
    unsigned int height = 0;
    int ErrorStatus;
    unsigned int dum;
    int dummy;
    Window win;

    /*
     * get geometry 
     */
    if (attributes && attributes->valuemask & XpmSize) {
	width = attributes->width;
	height = attributes->height;
    } else {
	if (pixmap)
	    XGetGeometry(display, pixmap, &win, &dummy, &dummy,
			 &width, &height, &dum, &dum);
	else if (shapemask)
	    XGetGeometry(display, shapemask, &win, &dummy, &dummy,
			 &width, &height, &dum, &dum);
    }

    /*
     * get the images 
     */
    if (pixmap)
	image = XGetImage(display, pixmap, 0, 0, width, height,
			  AllPlanes, ZPixmap);
    if (shapemask)
	shapeimage = XGetImage(display, shapemask, 0, 0, width, height,
			       AllPlanes, ZPixmap);

    /*
     * create data from images 
     */
    ErrorStatus = _XmXpmCreateDataFromImage(display, data_return, image,
					 shapeimage, attributes);
    if (image)
	XDestroyImage(image);
    if (shapeimage)
	XDestroyImage(shapeimage);

    return (ErrorStatus);
}
