/*
 * @(#)WGLSurfaceData.java	1.14 10/03/23
 *
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.java2d.opengl;

import java.awt.Component;
import java.awt.GraphicsConfiguration;
import java.awt.GraphicsDevice;
import java.awt.GraphicsEnvironment;
import java.awt.Image;
import java.awt.Rectangle;
import java.awt.image.ColorModel;
import sun.awt.SunToolkit;
import sun.awt.windows.WComponentPeer;
import sun.java2d.SurfaceData;

public abstract class WGLSurfaceData extends OGLSurfaceData {

    protected WComponentPeer peer;
    private WGLGraphicsConfig graphicsConfig;

    private native void initOps(long pConfigInfo, WComponentPeer peer,
                                long hwnd);
    protected native boolean initPbuffer(long pData, long pConfigInfo,
                                         boolean isOpaque,
                                         int width, int height);

    protected WGLSurfaceData(WComponentPeer peer, WGLGraphicsConfig gc,
                             ColorModel cm, int type)
    {
        super(gc, cm, type);
	this.peer = peer;
	this.graphicsConfig = gc;

        long pConfigInfo = gc.getNativeConfigInfo();
        long hwnd = peer != null ? peer.getHWnd() : 0L;

	initOps(pConfigInfo, peer, hwnd);
    }

    public GraphicsConfiguration getDeviceConfiguration() {
        return graphicsConfig;
    }

    /**
     * Creates a SurfaceData object representing the primary (front) buffer
     * of an on-screen Window.
     */
    public static WGLWindowSurfaceData createData(WComponentPeer peer) {
        // the OGL pipeline can render directly to the screen and interfere
        // with layered windows, which is why we don't allow accelerated
        // surfaces in this case
        if (!peer.isAccelCapable() ||
            !SunToolkit.isContainingTopLevelOpaque((Component)peer.getTarget()))
        {
            return null;
        }
        WGLGraphicsConfig gc = getGC(peer);
        return new WGLWindowSurfaceData(peer, gc);
    }

    /**
     * Creates a SurfaceData object representing the back buffer of a
     * double-buffered on-screen Window.
     */
    public static WGLOffScreenSurfaceData createData(WComponentPeer peer,
                                                     Image image,
                                                     int type)
    {
        // the OGL pipeline can render directly to the screen and interfere
        // with layered windows, which is why we don't allow accelerated
        // surfaces in this case
        if (!peer.isAccelCapable() ||
            !SunToolkit.isContainingTopLevelOpaque((Component)peer.getTarget()))
        {
            return null;
        }
        WGLGraphicsConfig gc = getGC(peer);
        Rectangle r = peer.getBounds();
        if (type == FLIP_BACKBUFFER) {
            return new WGLOffScreenSurfaceData(peer, gc, r.width, r.height,
                                               image, peer.getColorModel(),
                                               type);
        } else {
            return new WGLVSyncOffScreenSurfaceData(peer, gc, r.width, r.height,
                                                    image, peer.getColorModel(),
                                                    type);
        }
    }

    /**
     * Creates a SurfaceData object representing an off-screen buffer (either
     * a Pbuffer or Texture).
     */
    public static WGLOffScreenSurfaceData createData(WGLGraphicsConfig gc,
                                                     int width, int height,
                                                     ColorModel cm, 
                                                     Image image, int type)
    {
	return new WGLOffScreenSurfaceData(null, gc, width, height,
                                           image, cm, type);
    }

    public static WGLGraphicsConfig getGC(WComponentPeer peer) {
	if (peer != null) {
	    return (WGLGraphicsConfig)peer.getGraphicsConfiguration();
	} else {
            // REMIND: this should rarely (never?) happen, but what if
            //         default config is not WGL?
	    GraphicsEnvironment env =
		GraphicsEnvironment.getLocalGraphicsEnvironment();
	    GraphicsDevice gd = env.getDefaultScreenDevice();
	    return (WGLGraphicsConfig)gd.getDefaultConfiguration();
	}
    }

    public static class WGLWindowSurfaceData extends WGLSurfaceData {

	public WGLWindowSurfaceData(WComponentPeer peer, 
                                    WGLGraphicsConfig gc)
        {
            super(peer, gc, peer.getColorModel(), WINDOW);
	}

	public SurfaceData getReplacement() {
	    return peer.getSurfaceData();
	}

	public Rectangle getBounds() {
            Rectangle r = peer.getBounds();
            r.x = r.y = 0;
            return r;
	}

	/**
	 * Returns destination Component associated with this SurfaceData.
	 */
	public Object getDestination() {
	    return peer.getTarget();
	}
    }

    /**
     * A surface which implements a v-synced flip back-buffer with COPIED
     * FlipContents.
     *
     * This surface serves as a back-buffer to the outside world, while
     * it is actually an offscreen surface. When the BufferStrategy this surface
     * belongs to is showed, it is first copied to the real private
     * FLIP_BACKBUFFER, which is then flipped.
     */
    public static class WGLVSyncOffScreenSurfaceData extends
        WGLOffScreenSurfaceData
    {
        private WGLOffScreenSurfaceData flipSurface;

	public WGLVSyncOffScreenSurfaceData(WComponentPeer peer,
                                            WGLGraphicsConfig gc,
                                            int width, int height,
                                            Image image, ColorModel cm,
                                            int type)
        {
            super(peer, gc, width, height, image, cm, type);
            flipSurface = WGLSurfaceData.createData(peer, image, FLIP_BACKBUFFER);
        }

        public SurfaceData getFlipSurface() {
            return flipSurface;
        }

        @Override
        public void flush() {
            flipSurface.flush();
            super.flush();
        }

    }

    public static class WGLOffScreenSurfaceData extends WGLSurfaceData {

	private Image offscreenImage;
	private int width, height;

	public WGLOffScreenSurfaceData(WComponentPeer peer,
                                       WGLGraphicsConfig gc, 
                                       int width, int height,
                                       Image image, ColorModel cm,
                                       int type)
        {
            super(peer, gc, cm, type);

            this.width = width;
            this.height = height;
            offscreenImage = image;

            initSurface(width, height);
	}

	public SurfaceData getReplacement() {
            return restoreContents(offscreenImage);
	}

	public Rectangle getBounds() {
            if (type == FLIP_BACKBUFFER) {
                Rectangle r = peer.getBounds();
                r.x = r.y = 0;
                return r;
            } else {
                return new Rectangle(width, height);
            }
	}

	/**
	 * Returns destination Image associated with this SurfaceData.
	 */
	public Object getDestination() {
	    return offscreenImage;
	}
    }

    /**
     * Updates the layered window with the contents of the surface.
     *
     * @param psdops pointer to the native ogl sd structure
     * @param pData pointer to the AwtWindow peer data
     * @param w width of the window
     * @param h height of the window
     * @see sun.awt.windows.TranslucentWindowPainter
     */
    public static native boolean updateWindowAccelImpl(long psdops,
                                                       WComponentPeer peer,
                                                       int w, int h);
}
