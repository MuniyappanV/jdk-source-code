/*
 * @(#)awt_GraphicsEnv.c	1.55 04/02/17
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include "awt_p.h"
#include "awt.h"
#include "color.h"
#include <sun_awt_X11GraphicsEnvironment.h>
#include <sun_awt_X11GraphicsDevice.h>
#include <sun_awt_X11GraphicsConfig.h>
#ifndef HEADLESS
#include <X11/extensions/Xdbe.h>
#endif /* !HEADLESS */

#include <jni.h>
#include <jni_util.h>
#include <jvm.h>

#include <stdlib.h>

#include "awt_GraphicsEnv.h"
#include "awt_Window.h"
#include "awt_util.h"
#include "gdefs.h"
#include <dlfcn.h>

#ifdef NETSCAPE
#include <signal.h>
extern int awt_init_xt;
#endif

#ifndef HEADLESS
#ifdef __linux__
#include <X11/XKBlib.h>
#include "Xinerama.h"
#endif

int awt_numScreens;     /* Xinerama-aware number of screens */

AwtScreenDataPtr x11Screens;
#endif /* !HEADLESS */

#ifdef HEADLESS
#define Display void
#endif /* HEADLESS */

Display *awt_display;


struct X11GraphicsConfigIDs x11GraphicsConfigIDs;
struct X11GraphicsDeviceIDs x11GraphicsDeviceIDs;
extern struct WindowIDs mWindowIDs;
extern struct MWindowPeerIDs mWindowPeerIDs;

#ifndef HEADLESS
int awtCreateX11Colormap(AwtGraphicsConfigDataPtr adata);
#endif /* HEADLESS */

static char *x11GraphicsConfigClassName = "sun/awt/X11GraphicsConfig";

/* AWT and Xinerama
 *
 * As of fix 4356756, AWT is Xinerama-aware.  X11GraphicsDevices are created for
 * each screen of a Xinerama setup, though X11 itself still only sees a single
 * display.
 * In many places where we talk to X11, a xinawareScreen variable is used to
 * pass the correct Display value, depending on the circumstances (a single
 * X display, multiple X displays, or a single X display with multiple
 * Xinerama screens).
 *
 * Solaris and Linux differ in the functions used to access Xinerama-related
 * data.  This is in part because at this time, the X consortium has not
 * finalized the "official" Xinerama API.  Once this spec is available, and
 * both OSes are conformant, one code base should be sufficient for Xinerama
 * operation on both OSes.  Until then, some of the Xinerama-related code
 * is ifdef'd appropriately.  -bchristi, 7/12/01
 */

#define MAXFRAMEBUFFERS 16
#ifndef __linux__ /* SOLARIS */
typedef Status XineramaGetInfoFunc(Display* display, int screen_number,
         XRectangle* framebuffer_rects, unsigned char* framebuffer_hints,
         int* num_framebuffers);
typedef Status XineramaGetCenterHintFunc(Display* display, int screen_number,
                                         int* x, int* y);

XineramaGetCenterHintFunc* XineramaSolarisCenterFunc = NULL;
#endif

Bool usingXinerama = False;
XRectangle fbrects[MAXFRAMEBUFFERS];

JNIEXPORT void JNICALL
Java_sun_awt_X11GraphicsConfig_initIDs (JNIEnv *env, jclass cls)
{
    x11GraphicsConfigIDs.aData = NULL;
    x11GraphicsConfigIDs.bitsPerPixel = NULL;
    x11GraphicsConfigIDs.screen = NULL;

    x11GraphicsConfigIDs.aData = (*env)->GetFieldID (env, cls, "aData", "J");
    x11GraphicsConfigIDs.bitsPerPixel = (*env)->GetFieldID (env, cls, "bitsPerPixel", "I");
    x11GraphicsConfigIDs.screen = (*env)->GetFieldID (env, cls, "screen", "Lsun/awt/X11GraphicsDevice;");

    if (x11GraphicsConfigIDs.aData == NULL || 
	    x11GraphicsConfigIDs.bitsPerPixel == NULL ||
        x11GraphicsConfigIDs.screen == NULL) {

	    JNU_ThrowNoSuchFieldError(env, "Can't find a field");
	    return;
	}
} 

JNIEXPORT void JNICALL
Java_sun_awt_X11GraphicsDevice_initIDs (JNIEnv *env, jclass cls)
{
    x11GraphicsDeviceIDs.screen = NULL;
    x11GraphicsDeviceIDs.screen = (*env)->GetFieldID (env, cls, "screen", "I");
    DASSERT(x11GraphicsDeviceIDs.screen);
}

#ifndef HEADLESS
/*
 * error handlers
 */

int
xerror_handler(Display * disp, XErrorEvent * err)
{
/* #ifdef DEBUG */
    char msg[128];
    char buf[128];
    char *ev = getenv("NOISY_AWT");

    if (!ev || !ev[0])
        return 0;
    XGetErrorText(disp, err->error_code, msg, sizeof(msg));
    jio_fprintf(stderr, "Xerror %s, XID %x, ser# %d\n", msg, err->resourceid, err->serial);
    jio_snprintf(buf, sizeof(buf), "%d", err->request_code);
    XGetErrorDatabaseText(disp, "XRequest", buf, "Unknown", msg, sizeof(msg));
    jio_fprintf(stderr, "Major opcode %d (%s)\n", err->request_code, msg);
    if (err->request_code > 128) {
        jio_fprintf(stderr, "Minor opcode %d\n", err->minor_code);
    }
    if (awt_lock) {
        /*SignalError(lockedee->lastpc, lockedee, "fp/ade/gui/GUIException", msg); */
    }
    if (strcasecmp(ev, "abort") == 0) {
        JNIEnv *env = (JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2);

        (*env)->FatalError(env, "xerror_handler abort");
    }
/* #endif */
    return 0;
}

static int 
xioerror_handler(Display * disp)
{
    if (awt_lock) {
        if (errno == EPIPE) {
            jio_fprintf(stderr, "X connection to %s host broken (explicit kill or server shutdown)\n", XDisplayName(NULL));
        }
        /*SignalError(lockedee->lastpc, lockedee, "fp/ade/gui/GUIException", "I/O error"); */
    }
    return 0;
}

static AwtGraphicsConfigDataPtr 
findWithTemplate(XVisualInfo *vinfo,
		 long mask)
{

    XVisualInfo *visualList;
    XColor color;
    AwtGraphicsConfigDataPtr defaultConfig;
    int visualsMatched, i;
    
    visualList = XGetVisualInfo(awt_display,
				mask, vinfo, &visualsMatched);
    if (visualList) {
	defaultConfig = ZALLOC(_AwtGraphicsConfigData);
	for (i = 0; i < visualsMatched; i++) {
	    memcpy(&defaultConfig->awt_visInfo, &visualList[i], sizeof(XVisualInfo));
	    defaultConfig->awt_depth = visualList[i].depth;

	    /* we can't use awtJNI_CreateColorData here, because it'll pull,
	       SystemColor, which in turn will cause toolkit to be reinitialized */
	    if (awtCreateX11Colormap(defaultConfig)) {
		/* Allocate white and black pixels for this visual */
		color.flags = DoRed | DoGreen | DoBlue;
		color.red = color.green = color.blue = 0x0000;
		XAllocColor(awt_display, defaultConfig->awt_cmap, &color);
		x11Screens[visualList[i].screen].blackpixel = color.pixel;
		color.flags = DoRed | DoGreen | DoBlue;
		color.red = color.green = color.blue = 0xffff;
		XAllocColor(awt_display, defaultConfig->awt_cmap, &color);
		x11Screens[visualList[i].screen].whitepixel = color.pixel;

		XFree(visualList);
		return defaultConfig;
	    }
	}
	XFree(visualList);
	free((void *)defaultConfig);
    }
    return NULL;
}

/* default config is based on X11 screen.  All Xinerama screens of that X11
   screen will have the same default config */
/* Need more notes about which fields of the structure are based on the X
   screen, and which are based on the Xinerama screen */
static AwtGraphicsConfigDataPtr
makeDefaultConfig(JNIEnv *env, int screen) {

    AwtGraphicsConfigDataPtr defaultConfig;
    int xinawareScreen = 0;
    VisualID forcedVisualID, defaultVisualID;
    char *forcedVisualStr;
    XVisualInfo vinfo;
    long mask;

    xinawareScreen = usingXinerama ? 0 : screen;
    defaultVisualID = 
	XVisualIDFromVisual(DefaultVisual(awt_display, xinawareScreen));
    vinfo.screen = xinawareScreen;

    if ((forcedVisualStr = getenv("FORCEDEFVIS"))) {
	mask = VisualIDMask | VisualScreenMask;
	if (sscanf(forcedVisualStr, "%x", &forcedVisualID) > 0 &&
	    forcedVisualID > 0) 
	{
	    vinfo.visualid = forcedVisualID;
	} else {
	    vinfo.visualid = defaultVisualID;
	}
    } else {
	vinfo.depth = 24;
	vinfo.class = TrueColor;
	mask = VisualDepthMask | VisualScreenMask | VisualClassMask;
    }

    /* try the best, or forced visual */
    defaultConfig = findWithTemplate(&vinfo, mask);
    if (defaultConfig) {
	return defaultConfig;
    }
    
    /* try the default visual if we haven't already */
    if (vinfo.visualid != defaultVisualID) {
	vinfo.visualid = defaultVisualID;
	mask = VisualIDMask | VisualScreenMask;
	defaultConfig = findWithTemplate(&vinfo, mask);
	if (defaultConfig) {
	    return defaultConfig;
	}
    }

    /* try any TrueColor */
    vinfo.class = TrueColor;
    mask = VisualScreenMask | VisualClassMask;
    defaultConfig = findWithTemplate(&vinfo, mask);
    if (defaultConfig) {
	return defaultConfig;
    }

    /* try 8-bit PseudoColor */
    vinfo.depth = 8;
    vinfo.class = PseudoColor;
    mask = VisualDepthMask | VisualScreenMask | VisualClassMask;
    defaultConfig = findWithTemplate(&vinfo, mask);
    if (defaultConfig) {
	return defaultConfig;
    }

    /* try any 8-bit */
    vinfo.depth = 8;
    mask = VisualDepthMask | VisualScreenMask;
    defaultConfig = findWithTemplate(&vinfo, mask);
    if (defaultConfig) {
	return defaultConfig;
    }

    /* we tried everything, give up */
    JNU_ThrowInternalError(env, "Can't find supported visual");
    XCloseDisplay(awt_display);
    awt_display = NULL;
    return NULL;
}

static void
getAllConfigs (JNIEnv *env, int screen, AwtScreenDataPtr screenDataPtr) {

    int i;
    int n8p=0, n12p=0, n8s=0, n8gs=0, n8sg=0, n1sg=0, nTrue=0;
    int nConfig;
    XVisualInfo *pVI8p, *pVI12p, *pVI8s, *pVITrue, *pVI8gs,
                *pVI8sg, *pVI1sg = NULL, viTmp;
    AwtGraphicsConfigDataPtr *graphicsConfigs;
    AwtGraphicsConfigDataPtr defaultConfig;
    int ind;
    char errmsg[128];
    int xinawareScreen;

    if (usingXinerama) {
        xinawareScreen = 0;
    }
    else {
        xinawareScreen = screen;
    }

    AWT_LOCK ();

    viTmp.screen = xinawareScreen;
    
    viTmp.depth = 8;
    viTmp.class = PseudoColor;
    viTmp.colormap_size = 256;
    pVI8p = XGetVisualInfo (awt_display, 
                            VisualDepthMask | VisualClassMask |
                            VisualColormapSizeMask | VisualScreenMask, 
                            &viTmp, &n8p);

    viTmp.depth = 12;
    viTmp.class = PseudoColor;
    viTmp.colormap_size = 4096;
    pVI12p = XGetVisualInfo (awt_display, 
			     VisualDepthMask | VisualClassMask |
			     VisualColormapSizeMask | VisualScreenMask, 
			     &viTmp, &n12p);
    
    viTmp.class = TrueColor;
    pVITrue = XGetVisualInfo (awt_display,
                              VisualClassMask |
                              VisualScreenMask,
                              &viTmp, &nTrue);
    
    viTmp.depth = 8;
    viTmp.class = StaticColor;
    pVI8s = XGetVisualInfo (awt_display, VisualDepthMask | VisualClassMask |
                            VisualScreenMask, &viTmp, &n8s);

    viTmp.depth = 8;
    viTmp.class = GrayScale;
    viTmp.colormap_size = 256;
    pVI8gs = XGetVisualInfo (awt_display, 
                             VisualDepthMask | VisualClassMask |
                             VisualColormapSizeMask | VisualScreenMask,
                             &viTmp, &n8gs);
    viTmp.depth = 8;
    viTmp.class = StaticGray;
    viTmp.colormap_size = 256;
    pVI8sg = XGetVisualInfo (awt_display, 
                             VisualDepthMask | VisualClassMask |
                             VisualColormapSizeMask | VisualScreenMask,
                             &viTmp, &n8sg);

/* REMIND.. remove when we have support for the color classes below */
/*     viTmp.depth = 1; */
/*     viTmp.class = StaticGray; */
/*     pVI1sg = XGetVisualInfo (awt_display, VisualDepthMask | VisualClassMask, */
/*                              viTmp, &n1sg); */
    
    nConfig = n8p + n12p + n8s + n8gs + n8sg  + n1sg + nTrue + 1;
    graphicsConfigs = (AwtGraphicsConfigDataPtr *)
        calloc(nConfig, sizeof(AwtGraphicsConfigDataPtr));
    if (graphicsConfigs == NULL) {
	JNU_ThrowOutOfMemoryError((JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2),
                                  NULL);
    }

    defaultConfig = screenDataPtr->defaultConfig;
    graphicsConfigs[0] = defaultConfig;
    nConfig = 1; /* reserve index 0 for default config */

    for (i = 0; i < nTrue; i++) {
        if (XVisualIDFromVisual(pVITrue[i].visual) ==
            XVisualIDFromVisual(defaultConfig->awt_visInfo.visual) ||
	    pVITrue[i].depth == 12) {
	    /* Skip the non-supported 12-bit TrueColor visual */
            continue;
        } else {
            ind = nConfig++;
        }
        graphicsConfigs [ind] = ZALLOC (_AwtGraphicsConfigData);
	graphicsConfigs [ind]->primitiveType = -255;
        graphicsConfigs [ind]->awt_depth = pVITrue [i].depth;
        memcpy (&graphicsConfigs [ind]->awt_visInfo, &pVITrue [i],
                sizeof (XVisualInfo));
    }
 
    for (i = 0; i < n8p; i++) {
        if (XVisualIDFromVisual(pVI8p[i].visual) ==
            XVisualIDFromVisual(defaultConfig->awt_visInfo.visual)) {
            continue;
        } else {
            ind = nConfig++;
        }
        graphicsConfigs [ind] = ZALLOC (_AwtGraphicsConfigData);
	graphicsConfigs [ind]->primitiveType = -255;
        graphicsConfigs [ind]->awt_depth = pVI8p [i].depth;
        memcpy (&graphicsConfigs [ind]->awt_visInfo, &pVI8p [i],
                sizeof (XVisualInfo));
    }
       
    for (i = 0; i < n12p; i++) {
        if (XVisualIDFromVisual(pVI12p[i].visual) ==
            XVisualIDFromVisual(defaultConfig->awt_visInfo.visual)) {
            continue;
        } else {
            ind = nConfig++;
        }
        graphicsConfigs [ind] = ZALLOC (_AwtGraphicsConfigData);
	graphicsConfigs [ind]->primitiveType = -255;
        graphicsConfigs [ind]->awt_depth = pVI12p [i].depth;
        memcpy (&graphicsConfigs [ind]->awt_visInfo, &pVI12p [i],
                sizeof (XVisualInfo));
    }
       
    for (i = 0; i < n8s; i++) {
        if (XVisualIDFromVisual(pVI8s[i].visual) ==
            XVisualIDFromVisual(defaultConfig->awt_visInfo.visual)) {
            continue;
        } else {
            ind = nConfig++;
        }
        graphicsConfigs [ind] = ZALLOC (_AwtGraphicsConfigData);
	graphicsConfigs [ind]->primitiveType = -255;
        graphicsConfigs [ind]->awt_depth = pVI8s [i].depth;
        memcpy (&graphicsConfigs [ind]->awt_visInfo, &pVI8s [i],
                sizeof (XVisualInfo));
    }

    for (i = 0; i < n8gs; i++) {
        if (XVisualIDFromVisual(pVI8gs[i].visual) ==
            XVisualIDFromVisual(defaultConfig->awt_visInfo.visual)) {
            continue;
        } else {
            ind = nConfig++;
        }
        graphicsConfigs [ind] = ZALLOC (_AwtGraphicsConfigData);
	graphicsConfigs [ind]->primitiveType = -255;
        graphicsConfigs [ind]->awt_depth = pVI8gs [i].depth;
        memcpy (&graphicsConfigs [ind]->awt_visInfo, &pVI8gs [i],
                sizeof (XVisualInfo));
    }

    for (i = 0; i < n8sg; i++) {
        if (XVisualIDFromVisual(pVI8sg[i].visual) ==
            XVisualIDFromVisual(defaultConfig->awt_visInfo.visual)) {
            continue;
        } else {
            ind = nConfig++;
        }
        graphicsConfigs [ind] = ZALLOC (_AwtGraphicsConfigData);
	graphicsConfigs [ind]->primitiveType = -255;
        graphicsConfigs [ind]->awt_depth = pVI8sg [i].depth;
        memcpy (&graphicsConfigs [ind]->awt_visInfo, &pVI8sg [i],
                sizeof (XVisualInfo));
    }

    for (i = 0; i < n1sg; i++) {
        if (XVisualIDFromVisual(pVI1sg[i].visual) ==
            XVisualIDFromVisual(defaultConfig->awt_visInfo.visual)) {
            continue;
        } else {
            ind = nConfig++;
        }
        graphicsConfigs [ind] = ZALLOC (_AwtGraphicsConfigData);
	graphicsConfigs [ind]->primitiveType = -255;
        graphicsConfigs [ind]->awt_depth = pVI1sg [i].depth;
        memcpy (&graphicsConfigs [ind]->awt_visInfo, &pVI1sg [i],
                sizeof (XVisualInfo));
    }

    if (n8p != 0)
       XFree (pVI8p);
    if (n12p != 0)
       XFree (pVI12p);
    if (n8s != 0)
       XFree (pVI8s);
    if (n8gs != 0)
       XFree (pVI8gs);
    if (n8sg != 0)
       XFree (pVI8sg);
    if (n1sg != 0)
       XFree (pVI1sg);

    screenDataPtr->numConfigs = nConfig;
    screenDataPtr->configs = graphicsConfigs;

    AWT_UNLOCK ();
}

/*
 * Determing if this top-level has been moved onto another Xinerama screen.
 * Called from awt_TopLevel.c
 *
 * ASSUME: wdata != null
 */
#ifndef HEADLESS
void checkNewXineramaScreen(JNIEnv* env, jobject peer, struct FrameData* wdata,
                            int32_t newX, int32_t newY, 
                            int32_t newWidth, int32_t newHeight) {
    int i;
    int amt;
    int totAmt = 0;
    int largestAmt = 0;
    int largestAmtScr = 0;

    int horiz;
    int vert;

    if (!usingXinerama) { return; }

    totAmt = newWidth * newHeight;

    /* assert that peer implements WindowPeer */
    DASSERT(JNU_IsInstanceOfByName(env, peer, "java/awt/peer/WindowPeer"));

    DTRACE_PRINTLN4("checkNewXineramaScreen() x=%i y=%i w=%i h=%i\n",newX, newY, newWidth, newHeight);

    /* decide which screen we're on
     * if we're spanning, figure out which screen we're most on
     */
    for (i = 0; i < awt_numScreens; i++) {
        if (INTERSECTS(newX, newX + newWidth, newY, newY + newHeight,
                       fbrects[i].x, fbrects[i].x + fbrects[i].width,
                       fbrects[i].y, fbrects[i].y + fbrects[i].height)) {

            /* calc how much of window is on this screen */
            horiz = MIN(newX + newWidth, fbrects[i].x + fbrects[i].width) - 
                    MAX(newX, fbrects[i].x);
            vert =  MIN(newY + newHeight, fbrects[i].y + fbrects[i].height) - 
                    MAX(newY, fbrects[i].y);
            DASSERT(horiz > 0);
            DASSERT(vert > 0);

            amt = horiz * vert;
            if (amt == totAmt) {
                /* completely on this screen - done! */
                largestAmtScr = i;
                break;
            }
            if (amt > largestAmt) {
                largestAmt = amt;
                largestAmtScr = i;
            }
        }
    }

#ifndef XAWT
    /* check if we're on a new screen */
    if (largestAmtScr != wdata->screenNum) {
        wdata->screenNum = largestAmtScr;
        /* update peer, target Comp */
        (*env)->CallVoidMethod(env, peer,
                               mWindowPeerIDs.draggedToScreenMID, largestAmtScr);
                               
    }
#endif /* XAWT */
}
#endif /* HEADLESS */

/*
 * Do Xinerama-related initialization such as
 * - check if Xinerama is running
 * - if so, load and run Xinerama query functions from the appropriate library
 */
#ifndef HEADLESS
void xineramaInit(void) {
#ifdef __linux__
    char* XinExtName = "XINERAMA";
    int32_t major_opcode, first_event, first_error;
    Bool gotXinExt = False;
    unsigned char fbhints[MAXFRAMEBUFFERS];
    int32_t locNumScr = 0;
    int32_t idx;

    XineramaScreenInfo *xinInfo;

    gotXinExt = XQueryExtension(awt_display, XinExtName, &major_opcode,
                                &first_event, &first_error);

    if (gotXinExt) {
        DTRACE_PRINTLN("Xinerama extension available");
                    DTRACE_PRINTLN("calling XineramaGetInfo func on Linux");
                    xinInfo = XineramaQueryScreens(awt_display, &locNumScr);
                    if (xinInfo != NULL) {
                        DTRACE_PRINTLN("Enabling Xinerama support");
                        usingXinerama = True;
                        /* set global number of screens */
                        DTRACE_PRINTLN1(" num screens = %i\n", locNumScr);
                        awt_numScreens = locNumScr;

                        /* stuff values into fbrects */
                        for (idx = 0; idx < awt_numScreens; idx++) {
                            DASSERT(xinInfo[idx].screen_number == idx);

                            fbrects[idx].width = xinInfo[idx].width;
                            fbrects[idx].height = xinInfo[idx].height;
                            fbrects[idx].x = xinInfo[idx].x_org;
                            fbrects[idx].y = xinInfo[idx].y_org;
                        }
                    } else {
                        DTRACE_PRINTLN("calling XineramaQueryScreens didn't work");
                    }
    } else {
        DTRACE_PRINTLN("Xinerama not available");
    }

#else /* Solaris */

    char* XinExtName = "XINERAMA";
    int32_t major_opcode, first_event, first_error;
    Bool gotXinExt = False;
    void* libHandle = 0;
    unsigned char fbhints[MAXFRAMEBUFFERS];
    int locNumScr = 0;

    char* XineramaLibName= "libXext.so";
    char* XineramaGetInfoName = "XineramaGetInfo";
    char* XineramaGetCenterHintName = "XineramaGetCenterHint";
    XineramaGetInfoFunc* XineramaSolarisFunc = NULL;

    gotXinExt = XQueryExtension(awt_display, XinExtName, &major_opcode,
                                &first_event, &first_error);

    if (gotXinExt) {
        DTRACE_PRINTLN("Xinerama extension available");

        /* load library, load and run XineramaGetInfo */
        libHandle = dlopen(XineramaLibName, RTLD_LAZY | RTLD_GLOBAL);
        if (libHandle != 0) {
            XineramaSolarisFunc = (XineramaGetInfoFunc*)dlsym(libHandle, XineramaGetInfoName);
            XineramaSolarisCenterFunc =
             (XineramaGetCenterHintFunc*)dlsym(libHandle,
                                               XineramaGetCenterHintName);
            if (XineramaSolarisFunc != NULL) {
                DTRACE_PRINTLN("calling XineramaGetInfo func on Solaris");
                if ((*XineramaSolarisFunc)(awt_display, 0, &fbrects[0],
                     &fbhints[0], &locNumScr) != 0) {

                    DTRACE_PRINTLN("Enabling Xinerama support");
                    usingXinerama = True;
                    /* set global number of screens */
                    DTRACE_PRINTLN1(" num screens = %i\n", locNumScr);
                    awt_numScreens = locNumScr;
                } else {
                    DTRACE_PRINTLN("calling XineramaGetInfo didn't work");
                }
            } else {
                DTRACE_PRINTLN("couldn't load XineramaGetInfo symbol");
            }
            dlclose(libHandle);
        } else {
            DTRACE_PRINTLN1("\ncouldn't open shared library: %s\n", dlerror());
        }
    } else {
        DTRACE_PRINTLN("Xinerama not available");
    }
#endif /* __linux__ */
}
#endif /* HEADLESS */

Display *
awt_init_Display(JNIEnv *env, jobject this)
{
    Display *dpy;
    char errmsg[128];
    AwtGraphicsConfigData aData;
    int i;
#ifdef NETSCAPE
    sigset_t alarm_set, oldset;
#endif

    if (awt_display) {
        return awt_display;
    }

#ifdef NETSCAPE
    /* Disable interrupts during XtOpenDisplay to avoid bugs in unix os select
       code: some unix systems don't implement SA_RESTART properly and
       because of this, select returns with EINTR. Most implementations of
       gethostbyname don't cope with EINTR properly and as a result we get
       stuck (forever) in the gethostbyname code
    */
    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);
    sigprocmask(SIG_BLOCK, &alarm_set, &oldset);
#endif
    awt_lock = (*env)->NewGlobalRef(env, this);
 
#ifdef __linux__
  if (getenv("_AWT_IGNORE_XKB") != NULL &&
      strlen(getenv("_AWT_IGNORE_XKB")) > 0) {
      if (XkbIgnoreExtension(True)) {
	  printf("Ignoring XKB.\n");
      }
  }
#endif

    dpy = awt_display = XOpenDisplay(NULL);
#ifdef NETSCAPE
    sigprocmask(SIG_SETMASK, &oldset, NULL);
#endif
    if (!dpy) {
	jio_snprintf(errmsg,
		     sizeof(errmsg),
		     "Can't connect to X11 window server using '%s' as the value of the DISPLAY variable.",
		     (getenv("DISPLAY") == NULL) ? ":0.0" : getenv("DISPLAY"));
        JNU_ThrowInternalError(env, errmsg);
        return NULL;
    }

    XSetErrorHandler(xerror_handler);
    XSetIOErrorHandler(xioerror_handler);

    /* set awt_numScreens, and whether or not we're using Xinerama */
    xineramaInit();

    if (!usingXinerama) {
        awt_numScreens =  XScreenCount(awt_display);
    }
    
    DTRACE_PRINTLN1("allocating %i screens\n", awt_numScreens);
    /* Allocate screen data structure array */
    x11Screens = calloc(awt_numScreens, sizeof(AwtScreenData));
    if (x11Screens == NULL) {
        JNU_ThrowOutOfMemoryError((JNIEnv *)JNU_GetEnv(jvm, JNI_VERSION_1_2),
                                  NULL);
    }

    for (i = 0; i < awt_numScreens; i++) {
        if (usingXinerama) {
            /* All Xinerama screens use the same X11 root for now */
            x11Screens[i].root = RootWindow(awt_display, 0);
        }
        else {
            x11Screens[i].root = RootWindow(awt_display, i);
        }
        x11Screens[i].defaultConfig = makeDefaultConfig(env, i);
    }
    
    return dpy;
}
#endif /* !HEADLESS */

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    getDefaultScreenNum
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_X11GraphicsEnvironment_getDefaultScreenNum(
JNIEnv *env, jobject this)
{
#ifdef HEADLESS
    return (jint)0;
#else
    return DefaultScreen(awt_display);
#endif /* !HEADLESS */
}

#ifdef HEADLESS
void* getDefaultConfig(int screen) {
    return NULL;
}
#else
AwtGraphicsConfigDataPtr
getDefaultConfig(int screen) {
    return x11Screens[screen].defaultConfig;
}

AwtScreenDataPtr
getScreenData(int screen) {
    return &(x11Screens[screen]);
}
#endif /* !HEADLESS */
 
/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    initDisplay
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_sun_awt_X11GraphicsEnvironment_initDisplay(JNIEnv *env, jobject this)
{
#ifndef HEADLESS
    (void) awt_init_Display(env, this);
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    getNumScreens
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_X11GraphicsEnvironment_getNumScreens(JNIEnv *env, jobject this)
{
#ifdef HEADLESS
    return (jint)0;
#else
    return awt_numScreens;
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsDevice
 * Method:    getDisplay
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_sun_awt_X11GraphicsDevice_getDisplay(JNIEnv *env, jobject this)
{
#ifdef HEADLESS
    return NULL;
#else
    return (jlong) awt_display;
#endif /* !HEADLESS */
}

#ifdef MITSHM

static jint canUseShmExt = UNSET_MITSHM;
static jint canUseShmExtPixmaps = UNSET_MITSHM;
static jboolean xshmAttachFailed = JNI_FALSE;

int J2DXErrHandler(Display *display, XErrorEvent *xerr) {
    int ret = 0;
    if (xerr->minor_code == X_ShmAttach) {
	xshmAttachFailed = JNI_TRUE;
    } else {
	ret = (*xerror_saved_handler)(display, xerr);
    }
    return ret;
}
jboolean isXShmAttachFailed() { 
    return xshmAttachFailed;
} 
void resetXShmAttachFailed() { 
    xshmAttachFailed = JNI_FALSE; 
}

void TryInitMITShm(JNIEnv *env, jint *shmExt, jint *shmPixmaps) {
    XShmSegmentInfo shminfo;
    int XShmMajor, XShmMinor;
    int a, b, c;

    AWT_LOCK();
    if (canUseShmExt != UNSET_MITSHM) {
	*shmExt = canUseShmExt;
	*shmPixmaps = canUseShmExtPixmaps;
	AWT_UNLOCK();
	return;
    }

    *shmExt = canUseShmExt = CANT_USE_MITSHM;
    *shmPixmaps = canUseShmExtPixmaps = CANT_USE_MITSHM;

    /** 
     * XShmQueryExtension returns False in remote server case.
     * Unfortunately it also returns True in ssh case, so 
     * we need to test that we can actually do XShmAttach.
     */
    if (XShmQueryExtension(awt_display)) {
	shminfo.shmid = shmget(IPC_PRIVATE, 0x10000, IPC_CREAT|0777);
	if (shminfo.shmid < 0) {
	    AWT_UNLOCK();
#ifdef DEBUG
	    perror("TryInitMITShm: shmget has failed");
#endif
	    return;
	}
	shminfo.shmaddr = (char *) shmat(shminfo.shmid, 0, 0);
	if (shminfo.shmaddr == ((char *) -1)) {
	    shmctl(shminfo.shmid, IPC_RMID, 0);
	    AWT_UNLOCK();
#ifdef DEBUG
	    perror("TryInitMITShm: shmat has failed");
#endif
	    return;
	}
	shminfo.readOnly = True;

	resetXShmAttachFailed();
	/**
	 * The J2DXErrHandler handler will set xshmAttachFailed 
	 * to JNI_TRUE if any Shm error has occured.
	 */
	EXEC_WITH_XERROR_HANDLER(J2DXErrHandler, 
				 XShmAttach(awt_display, &shminfo));

	/**
	 * Get rid of the id now to reduce chances of leaking 
	 * system resources. 
	 */ 
	shmctl(shminfo.shmid, IPC_RMID, 0); 

	if (isXShmAttachFailed() == JNI_FALSE) {
	    canUseShmExt = CAN_USE_MITSHM;
	    /* check if we can use shared pixmaps */
	    XShmQueryVersion(awt_display, &XShmMajor, &XShmMinor, 
			     (Bool*)&canUseShmExtPixmaps);
	    canUseShmExtPixmaps = canUseShmExtPixmaps && 
		(XShmPixmapFormat(awt_display) == ZPixmap);
	    XShmDetach(awt_display, &shminfo);
	}
	shmdt(shminfo.shmaddr);
	*shmExt = canUseShmExt;
	*shmPixmaps = canUseShmExtPixmaps;
    }
    AWT_UNLOCK();
}
#endif /* MITSHM */

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    checkShmExt
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_X11GraphicsEnvironment_checkShmExt(JNIEnv *env, jobject this)
{

    int shmExt = NOEXT_MITSHM, shmPixmaps;
#ifdef MITSHM
    TryInitMITShm(env, &shmExt, &shmPixmaps);
#endif
    return shmExt;
}

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    getDisplayString
 * Signature: ()Ljava/lang/String
 */
JNIEXPORT jstring JNICALL 
Java_sun_awt_X11GraphicsEnvironment_getDisplayString
  (JNIEnv *env, jobject this) 
{
#ifdef HEADLESS
    return (jstring)NULL;
#else
    return (*env)->NewStringUTF(env, DisplayString(awt_display));
#endif /* HEADLESS */
}


/*
 * Class:     sun_awt_X11GraphicsDevice
 * Method:    getNumConfigs
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_X11GraphicsDevice_getNumConfigs(
JNIEnv *env, jobject this, jint screen)
{
#ifdef HEADLESS
    return (jint)0;
#else
    if (x11Screens[screen].numConfigs == 0) {
	getAllConfigs (env, screen, &(x11Screens[screen]));
    }

    return x11Screens[screen].numConfigs;
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsDevice
 * Method:    getConfigVisualId
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_X11GraphicsDevice_getConfigVisualId(
JNIEnv *env, jobject this, jint index, jint screen)
{
#ifdef HEADLESS
    return (jint)0;
#else
    int visNum;
 
    if (index == 0) {
        return ((jint)x11Screens[screen].defaultConfig->awt_visInfo.visualid);
    } else {
        if (x11Screens[screen].numConfigs == 0) {
            getAllConfigs (env, screen, &(x11Screens[screen]));
        }
        return ((jint)x11Screens[screen].configs[index]->awt_visInfo.visualid);
    }
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsDevice
 * Method:    getConfigDepth
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_X11GraphicsDevice_getConfigDepth(
JNIEnv *env, jobject this, jint index, jint screen)
{
#ifdef HEADLESS
    return (jint)0;
#else
    int visNum;
 
    if (index == 0) {
        return ((jint)x11Screens[screen].defaultConfig->awt_visInfo.depth);
    } else {
        if (x11Screens[screen].numConfigs == 0) {
            getAllConfigs (env, screen, &(x11Screens[screen]));
        }
        return ((jint)x11Screens[screen].configs[index]->awt_visInfo.depth);
    }
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsDevice
 * Method:    getConfigColormap
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_X11GraphicsDevice_getConfigColormap(
JNIEnv *env, jobject this, jint index, jint screen)
{
#ifdef HEADLESS
    return (jint)0;
#else
    int visNum;
 
    if (index == 0) {
        return ((jint)x11Screens[screen].defaultConfig->awt_cmap);
    } else {
        if (x11Screens[screen].numConfigs == 0) {
            getAllConfigs (env, screen, &(x11Screens[screen]));
        }
        return ((jint)x11Screens[screen].configs[index]->awt_cmap);
    }
#endif /* !HEADLESS */
}


/*
 * Class:     sun_awt_X11GraphicsConfig
 * Method:    getXResolution
 * Signature: ()I
 */
JNIEXPORT jdouble JNICALL
Java_sun_awt_X11GraphicsConfig_getXResolution(
JNIEnv *env, jobject this, jint screen)
{
#ifdef HEADLESS
    return (jdouble)0;
#else
    return ((DisplayWidth(awt_display, screen) * 25.4) /
	    DisplayWidthMM(awt_display, screen));
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsConfig
 * Method:    getYResolution
 * Signature: ()I
 */
JNIEXPORT jdouble JNICALL
Java_sun_awt_X11GraphicsConfig_getYResolution(
JNIEnv *env, jobject this, jint screen)
{
#ifdef HEADLESS
    return (jdouble)0;
#else
    return ((DisplayHeight(awt_display, screen) * 25.4) /
	    DisplayHeightMM(awt_display, screen));
#endif /* !HEADLESS */
}


/*
 * Class:     sun_awt_X11GraphicsConfig
 * Method:    getNumColors
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_sun_awt_X11GraphicsConfig_getNumColors(
JNIEnv *env, jobject this)
{
#ifdef HEADLESS
    return (jint)0;
#else
    AwtGraphicsConfigData *adata;
 
    adata = (AwtGraphicsConfigData *) JNU_GetLongFieldAsPtr(env, this, 
					      x11GraphicsConfigIDs.aData);

    return adata->awt_num_colors;
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsConfig
 * Method:    init
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_X11GraphicsConfig_init(
JNIEnv *env, jobject this, jint visualNum, jint screen)
{
#ifndef HEADLESS    
    AwtGraphicsConfigData *adata = NULL;
    AwtScreenData asd = x11Screens[screen];
    int i, n;
    int depth;
    XImage * tempImage;

    /* If haven't gotten all of the configs yet, do it now. */
    if (asd.numConfigs == 0) {
	getAllConfigs (env, screen, &asd);
    }

    /* Check the graphicsConfig for this visual */
    for (i = 0; i < asd.numConfigs; i++) {
        AwtGraphicsConfigDataPtr agcPtr = asd.configs[i];
	if ((jint)agcPtr->awt_visInfo.visualid == visualNum) {
	   adata = agcPtr;
	   break;
	}
    }

    /* If didn't find the visual, throw an exception... */
    if (adata == (AwtGraphicsConfigData *) NULL) {
        JNU_ThrowIllegalArgumentException(env, "Unknown Visual Specified");
	return;
    }

    /*  adata->awt_cmap initialization has been deferred to 
     *  makeColorModel call 
     */

    JNU_SetLongFieldFromPtr(env, this, x11GraphicsConfigIDs.aData, adata);

    depth = adata->awt_visInfo.depth;
    tempImage = XCreateImage(awt_display,
			     adata->awt_visInfo.visual,
			     depth, ZPixmap, 0, NULL, 1, 1, 32, 0);
    adata->pixelStride = (tempImage->bits_per_pixel + 7) / 8;
    (*env)->SetIntField(env, this, x11GraphicsConfigIDs.bitsPerPixel, 
			(jint)tempImage->bits_per_pixel);
    XDestroyImage(tempImage);
#endif /* !HEADLESS */
}
    

    
/*
 * Class:     sun_awt_X11GraphicsConfig
 * Method:    makeColorModel
 * Signature: ()Ljava/awt/image/ColorModel
 */
JNIEXPORT jobject JNICALL
Java_sun_awt_X11GraphicsConfig_makeColorModel(
JNIEnv *env, jobject this)
{
#ifdef HEADLESS
    return NULL;
#else
    AwtGraphicsConfigData *adata;
    jobject colorModel;

    /*
     * If awt is not locked yet, return null since the toolkit is not
     * initialized yet.
     */
    if (awt_lock == NULL) {
        return NULL;
    }
    
    AWT_LOCK (); 

    adata = (AwtGraphicsConfigData *) JNU_GetLongFieldAsPtr(env, this, 
					      x11GraphicsConfigIDs.aData);

    /* If colormap entry of adata is NULL, need to create it now */
    if (adata->awt_cmap == (Colormap) NULL) {
	awtJNI_CreateColorData (env, adata, 1);
    }

    /* Make Color Model object for this GraphicsConfiguration */
    colorModel = awtJNI_GetColorModel (env, adata);

    AWT_UNLOCK ();

    return colorModel;
#endif /* !HEADLESS */
}

    
/*
 * Class:     sun_awt_X11GraphicsConfig
 * Method:    getBounds
 * Signature: ()Ljava/awt/Rectangle
 */
JNIEXPORT jobject JNICALL
Java_sun_awt_X11GraphicsConfig_pGetBounds(JNIEnv *env, jobject this, jint screen)
{
#ifdef HEADLESS
    return NULL;
#else
    jclass clazz;
    jmethodID mid;
    jobject bounds = NULL;
    AwtGraphicsConfigDataPtr adata;

    adata = (AwtGraphicsConfigDataPtr)
        JNU_GetLongFieldAsPtr(env, this, x11GraphicsConfigIDs.aData);
    
    clazz = (*env)->FindClass(env, "java/awt/Rectangle");
    mid = (*env)->GetMethodID(env, clazz, "<init>", "(IIII)V");
    if (mid != NULL) {
        if (usingXinerama) {
            bounds = (*env)->NewObject(env, clazz, mid, fbrects[screen].x,
                                                        fbrects[screen].y,
                                                        fbrects[screen].width,
                                                        fbrects[screen].height);
        }
        else {
            bounds = (*env)->NewObject(env, clazz, mid, 0, 0,
                                   DisplayWidth(awt_display,
                                                adata->awt_visInfo.screen),
                                   DisplayHeight(awt_display,
                                                 adata->awt_visInfo.screen));
        }
        
        if ((*env)->ExceptionOccurred(env)) {
            return NULL;
        }
    }
    return bounds;
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsConfig
 * Method:    createBackBuffer
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL
Java_sun_awt_X11GraphicsConfig_createBackBuffer
    (JNIEnv *env, jobject this, jlong window, jint swapAction)
{
    int32_t v1, v2;
    XdbeBackBuffer ret = (unsigned long) 0;
    Window w = (Window)window;
    AWT_LOCK();
    if (!XdbeQueryExtension(awt_display, &v1, &v2)) {
        JNU_ThrowByName(env, "java/lang/Exception",
                        "Could not query double-buffer extension");
        AWT_UNLOCK();
        return (jlong)0;
    }
    ret = XdbeAllocateBackBufferName(awt_display, w,
                                     (XdbeSwapAction)swapAction);
    AWT_FLUSH_UNLOCK();
    return (jlong)ret;
}

/*
 * Class:     sun_awt_X11GraphicsConfig
 * Method:    destroyBackBuffer
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_X11GraphicsConfig_destroyBackBuffer
    (JNIEnv *env, jobject this, jlong backBuffer)
{
    AWT_LOCK();
    XdbeDeallocateBackBufferName(awt_display, (XdbeBackBuffer)backBuffer);
    AWT_FLUSH_UNLOCK();
}

/*
 * Class:     sun_awt_X11GraphicsConfig
 * Method:    swapBuffers
 * Signature: (JI)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_X11GraphicsConfig_swapBuffers
    (JNIEnv *env, jobject this,
     jlong window, jint swapAction)
{
    XdbeSwapInfo swapInfo;

    AWT_LOCK();

    XdbeBeginIdiom(awt_display);
    swapInfo.swap_window = (Window)window;
    swapInfo.swap_action = (XdbeSwapAction)swapAction;
    if (!XdbeSwapBuffers(awt_display, &swapInfo, 1)) {
        JNU_ThrowInternalError(env, "Could not swap buffers");
    }
    XdbeEndIdiom(awt_display);

    AWT_FLUSH_UNLOCK();
}

/*
 * Class:     sun_awt_X11GraphicsDevice
 * Method:    isDBESupported
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_sun_awt_X11GraphicsDevice_isDBESupported(JNIEnv *env, jobject this)
{
#ifdef HEADLESS
    return JNI_FALSE;
#else
    int opcode = 0, firstEvent = 0, firstError = 0;
    jboolean ret;

    AWT_LOCK();
    ret = (jboolean)XQueryExtension(awt_display, "DOUBLE-BUFFER",
                                    &opcode, &firstEvent, &firstError);
    AWT_FLUSH_UNLOCK();
    return ret;
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsDevice
 * Method:    getDoubleBufferVisuals
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_sun_awt_X11GraphicsDevice_getDoubleBufferVisuals(JNIEnv *env,
    jobject this, jint screen)
{
#ifndef HEADLESS
    jclass clazz;
    jmethodID midAddVisual;
    Window rootWindow;
    int i, n = 1;
    XdbeScreenVisualInfo* visScreenInfo;
    int xinawareScreen;

    if (usingXinerama) {
        xinawareScreen = 0;
    }
    else {
        xinawareScreen = screen;
    }

    clazz = (*env)->GetObjectClass(env, this);
    midAddVisual = (*env)->GetMethodID(env, clazz, "addDoubleBufferVisual",
        "(I)V");
    
    AWT_LOCK();
    rootWindow = RootWindow(awt_display, xinawareScreen);
    visScreenInfo = XdbeGetVisualInfo(awt_display, &rootWindow, &n);
    if (visScreenInfo == NULL) {
        JNU_ThrowInternalError(env, "Could not get visual info");
        AWT_UNLOCK();
        return;
    }
    AWT_FLUSH_UNLOCK();
    for (i = 0; i < visScreenInfo->count; i++) {
        XdbeVisualInfo* visInfo = visScreenInfo->visinfo;
        (*env)->CallVoidMethod(env, this, midAddVisual, (visInfo[i]).visual);
    }
#endif /* !HEADLESS */
}

/*
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    pRunningXinerama
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_sun_awt_X11GraphicsEnvironment_pRunningXinerama(JNIEnv *env,
    jobject this)
{
#ifdef HEADLESS
    return false;
#else
    return usingXinerama;
#endif /* HEADLESS */
}

/*
 * Can return NULL.
 *
 * Class:     sun_awt_X11GraphicsEnvironment
 * Method:    getXineramaCenterPoint
 * Signature: ()Ljava/awt/Point
 */
JNIEXPORT jobject JNICALL
Java_sun_awt_X11GraphicsEnvironment_getXineramaCenterPoint(JNIEnv *env,
    jobject this)
{
    jobject point = NULL;
#ifndef HEADLESS    /* return NULL in HEADLESS, Linux */
#ifndef __linux__
    int x,y;

    AWT_LOCK();
    DASSERT(usingXinerama);
    if (XineramaSolarisCenterFunc != NULL) {
        (XineramaSolarisCenterFunc)(awt_display, 0, &x, &y);
        point = JNU_NewObjectByName(env, "java/awt/Point","(II)V", x, y);
        DASSERT(point);
    } else {
        DTRACE_PRINTLN("unable to call XineramaSolarisCenterFunc: symbol is null");
    }
    AWT_FLUSH_UNLOCK();
#endif /* __linux __ */
#endif /* HEADLESS */
    return point;
}
