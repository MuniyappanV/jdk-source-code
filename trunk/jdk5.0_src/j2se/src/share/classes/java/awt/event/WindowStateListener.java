/*
 * @(#)WindowStateListener.java	1.5 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.awt.event;

import java.util.EventListener;

/**
 * The listener interface for receiving window state events.
 * <p>
 * The class that is interested in processing a window state event
 * either implements this interface (and all the methods it contains)
 * or extends the abstract <code>WindowAdapter</code> class
 * (overriding only the methods of interest).
 * <p>
 * The listener object created from that class is then registered with
 * a window using the <code>Window</code>'s
 * <code>addWindowStateListener</code> method.  When the window's
 * state changes by virtue of being iconified, maximized etc., the
 * <code>windowStateChanged</code> method in the listener object is
 * invoked, and the <code>WindowEvent</code> is passed to it.
 * 
 * @see java.awt.event.WindowAdapter
 * @see java.awt.event.WindowEvent
 *
 * @since 1.4
 */
public interface WindowStateListener extends EventListener {
    /**
     * Invoked when window state is changed.
     */
    public void windowStateChanged(WindowEvent e);
}
