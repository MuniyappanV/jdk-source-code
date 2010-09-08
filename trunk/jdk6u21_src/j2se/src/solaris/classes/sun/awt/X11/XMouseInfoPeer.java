/*
 * @(#)XMouseInfoPeer.java	1.6 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.awt.X11;

import java.awt.Point;
import java.awt.Window;
import java.awt.GraphicsEnvironment;
import java.awt.GraphicsConfiguration;
import java.awt.GraphicsDevice;
import java.awt.peer.MouseInfoPeer;

public class XMouseInfoPeer implements MouseInfoPeer {

    /**
     * Package-private constructor to prevent instantiation.
     */
    XMouseInfoPeer() {
    }

    public int fillPointWithCoords(Point point) {
        long display = XToolkit.getDisplay();
        GraphicsEnvironment ge = GraphicsEnvironment.
                                     getLocalGraphicsEnvironment();
        GraphicsDevice[] gds = ge.getScreenDevices();
        int gdslen = gds.length;

        XToolkit.awtLock();
        try {
            for (int i = 0; i < gdslen; i++) {
                long screenRoot = XlibWrapper.RootWindow(display, i); 
                boolean pointerFound = XlibWrapper.XQueryPointer(
                                           display, screenRoot,
                                           XlibWrapper.larg1,  // root_return
                                           XlibWrapper.larg2,  // child_return
                                           XlibWrapper.larg3,  // xr_return 
                                           XlibWrapper.larg4,  // yr_return
                                           XlibWrapper.larg5,  // xw_return 
                                           XlibWrapper.larg6,  // yw_return
                                           XlibWrapper.larg7); // mask_return
                if (pointerFound) {
                    point.x = Native.getInt(XlibWrapper.larg3);
                    point.y = Native.getInt(XlibWrapper.larg4);
                    return i;
                }
            }
        } finally {
            XToolkit.awtUnlock();
        }

        // this should never happen
        assert false : "No pointer found in the system.";
        return 0;
    }

    public boolean isWindowUnderMouse(Window w) {
            
        long display = XToolkit.getDisplay();
        XQueryTree qt = null;

        // java.awt.Component.findUnderMouseInWindow checks that 
        // the peer is non-null by checking that the component
        // is showing.
 
        long contentWindow = ((XWindow)w.getPeer()).getContentWindow();

        XToolkit.awtLock();

        try {
            
            qt = new XQueryTree(contentWindow);
            qt.set_nchildren(0);
            qt.execute(); 

            boolean windowOnTheSameScreen = XlibWrapper.XQueryPointer(display, qt.get_parent(),
                                  XlibWrapper.larg1, // root_return
                                  XlibWrapper.larg8, // child_return
                                  XlibWrapper.larg3, // root_x_return
                                  XlibWrapper.larg4, // root_y_return
                                  XlibWrapper.larg5, // win_x_return
                                  XlibWrapper.larg6, // win_y_return
                                  XlibWrapper.larg7); //  mask_return

            long siblingWindow = Native.getWindow(XlibWrapper.larg8);
            return (siblingWindow == contentWindow && windowOnTheSameScreen);

        } finally {
            if (qt != null) qt.dispose();
            XToolkit.awtUnlock();
        }
    }
}

