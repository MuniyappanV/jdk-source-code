/*
 * @(#)XInputMethod.java	1.10 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.awt.X11;

import java.awt.AWTException;
import java.awt.Component;
import java.awt.Container;
import java.awt.Rectangle;
import java.awt.im.spi.InputMethodContext;
import java.awt.peer.ComponentPeer;
import sun.awt.X11InputMethod;

import java.util.logging.*;

/**
 * Input Method Adapter for XIM (without Motif)
 *
 * @version 1.10 03/23/10
 * @author JavaSoft International
 */
public class XInputMethod extends X11InputMethod {
    private static final Logger log = Logger.getLogger("sun.awt.X11.XInputMethod");

    public XInputMethod() throws AWTException {
	super();
    }

    public void setInputMethodContext(InputMethodContext context) {
	context.enableClientWindowNotification(this, true);
    }

    public void notifyClientWindowChange(Rectangle location) {
	XComponentPeer peer = (XComponentPeer)getPeer(clientComponentWindow);
	if (peer != null) {
	    adjustStatusWindow(peer.getContentWindow());
	}
    }

    protected boolean openXIM() {
	return openXIMNative(XToolkit.getDisplay());
    }

    protected boolean createXIC() {
	XComponentPeer peer = (XComponentPeer)getPeer(clientComponentWindow);
	if (peer == null) {
	    return false;
	}
	return createXICNative(peer.getContentWindow());
    }


    private static volatile long xicFocus = 0;

    protected void setXICFocus(ComponentPeer peer,
				    boolean value, boolean active) {
	if (peer == null) {
	    return;
	}
        xicFocus = ((XComponentPeer)peer).getContentWindow();
	setXICFocusNative(((XComponentPeer)peer).getContentWindow(),
			  value,
			  active);
    }

    public static long getXICFocus() {
        return xicFocus;
    }

/* XAWT_HACK  FIX ME!
   do NOT call client code!
*/
    protected Container getParent(Component client) {
	return client.getParent();
    }

    /**
     * Returns peer of the given client component. If the given client component
     * doesn't have peer, peer of the native container of the client is returned.
     */
    protected ComponentPeer getPeer(Component client) {
	XComponentPeer peer;

        if (log.isLoggable(Level.FINE)) log.fine("Client is " + client);
        peer = (XComponentPeer)XToolkit.targetToPeer(client);
	while (client != null && peer == null) {
	    client = getParent(client);
            peer = (XComponentPeer)XToolkit.targetToPeer(client);
	}
        if (log.isLoggable(Level.FINE)) {
            log.log(Level.FINE, "Peer is {0}, client is {1}",
                    new Object[] {String.valueOf(peer), String.valueOf(client)});
        }

	if (peer != null)
	    return peer;

	return null;
    }

    /*
     * Subclasses should override disposeImpl() instead of dispose(). Client
     * code should always invoke dispose(), never disposeImpl().
     */
    protected synchronized void disposeImpl() {
	super.disposeImpl();
	clientComponentWindow = null;
    }

    protected void awtLock() {
	XToolkit.awtLock();
    }

    protected void awtUnlock() {
	XToolkit.awtUnlock();
    }

    long getCurrentParentWindow() {
	return (long)((XWindow)clientComponentWindow.getPeer()).getContentWindow();
    }

    /*
     * Native methods
     */
    private native boolean openXIMNative(long display);
    private native boolean createXICNative(long window);
    private native void setXICFocusNative(long window,
				    boolean value, boolean active);
    private native void adjustStatusWindow(long window);
}
