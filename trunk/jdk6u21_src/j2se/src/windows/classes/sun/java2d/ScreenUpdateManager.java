/*
 * @(#)ScreenUpdateManager.java	1.5 10/03/23
 *
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.java2d;

import java.awt.Color;
import java.awt.Font;
import java.awt.Graphics2D;
import sun.awt.Win32GraphicsConfig;
import sun.awt.windows.WComponentPeer;
import sun.java2d.d3d.D3DScreenUpdateManager;
import sun.java2d.windows.WindowsFlags;

/**
 * This class handles the creation of on-screen surfaces and
 * corresponding graphics objects.
 *
 * By default it delegates the surface creation to the
 * particular GraphicsConfiguration classes.
 */
public class ScreenUpdateManager {
    private static ScreenUpdateManager theInstance;

    protected ScreenUpdateManager() {
    }

    /**
     * Creates a SunGraphics2D object for the surface,
     * given the parameters.
     *
     * @param sd surface data for which a graphics is to be created
     * @param peer peer which owns the surface
     * @param fgColor fg color to be used in the graphics
     * @param bgColor bg color to be used in the graphics
     * @param font font to be used in the graphics
     * @return a SunGraphics2D object for rendering to the passed surface
     */
    public synchronized Graphics2D createGraphics(SurfaceData sd,
            WComponentPeer peer, Color fgColor, Color bgColor, Font font)
    {
        return new SunGraphics2D(sd, fgColor, bgColor, font);
    }

    /**
     * Creates and returns the surface for the peer. This surface becomes
     * managed by this manager. To remove the surface from the managed list
     * {@code}dropScreenSurface(SurfaceData){@code} will need to be called.
     *
     * The default implementation delegates surface creation
     * to the passed in GraphicsConfiguration object.
     *
     * @param gc graphics configuration for which the surface is to be created
     * @param peer peer for which the onscreen surface is to be created
     * @param bbNum number of back-buffers requested for this peer
     * @param isResize whether this surface is being created in response to
     * a component resize event
     * @return a SurfaceData to be used for on-screen rendering for this peer.
     * @see #dropScreenSurface(SurfaceData)
     */
    public SurfaceData createScreenSurface(Win32GraphicsConfig gc,
                                           WComponentPeer peer, int bbNum,
                                           boolean isResize)
    {
        return gc.createSurfaceData(peer, bbNum);
    }

    /**
     * Drops the passed surface from the list of managed surfaces.
     *
     * Nothing happens if the surface wasn't managed by this manager.
     *
     * @param sd SurfaceData to be removed from the list of managed surfaces
     */
    public void dropScreenSurface(SurfaceData sd) {}

    /**
     * Returns a replacement SurfaceData for the invalid passed one.
     *
     * This method should be used by SurfaceData's created by
     * the ScreenUpdateManager for providing replacement surfaces.
     *
     * @param peer to which the old surface belongs
     * @param oldsd the old (invalid) surface to get replaced
     * @return a replacement surface
     * @see sun.java2d.d3d.D3DSurfaceData.D3DWindowSurfaceData#getReplacement()
     * @see sun.java2d.windows.GDIWindowSurfaceData#getReplacement()
     */
    public SurfaceData getReplacementScreenSurface(WComponentPeer peer,
                                                   SurfaceData oldsd)
    {
        return peer.getSurfaceData();
    }

    /**
     * Returns an (singleton) instance of the screen surfaces
     * manager class.
     * @return instance of onscreen surfaces manager
     */
    public static synchronized ScreenUpdateManager getInstance() {
        if (theInstance == null) {
            if (WindowsFlags.isD3DEnabled()) {
                theInstance = new D3DScreenUpdateManager();
            } else {
                theInstance = new ScreenUpdateManager();
            }
        }
        return theInstance;
    }
}
