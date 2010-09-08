/*
 * @(#)WComponentPeer.java	1.183 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package sun.awt.windows;

import java.awt.*;
import java.awt.peer.*;
import java.awt.image.VolatileImage;
import sun.awt.RepaintArea;
import sun.awt.CausedFocusEvent;
import sun.awt.image.SunVolatileImage;
import sun.awt.image.ToolkitImage;
import java.awt.image.BufferedImage;
import java.awt.image.ImageProducer;
import java.awt.image.ImageObserver;
import java.awt.image.ColorModel;
import java.awt.event.PaintEvent;
import java.awt.event.InvocationEvent;
import java.awt.event.KeyEvent;
import sun.awt.Win32GraphicsConfig;
import sun.java2d.InvalidPipeException;
import sun.java2d.SurfaceData;
import static sun.java2d.d3d.D3DSurfaceData.*;
import sun.java2d.ScreenUpdateManager;
import sun.java2d.opengl.OGLSurfaceData;
import sun.java2d.pipe.Region;
import sun.awt.AWTAccessor;
import sun.awt.DisplayChangedListener;
import sun.awt.PaintEventDispatcher;
import sun.awt.SunToolkit;
import sun.awt.Win32GraphicsEnvironment;
import sun.awt.event.IgnorePaintEvent;

import java.awt.dnd.DropTarget;
import java.awt.dnd.peer.DropTargetPeer;

import sun.awt.DebugHelper;

import java.util.logging.*;

public abstract class WComponentPeer extends WObjectPeer 
    implements ComponentPeer, DropTargetPeer, DisplayChangedListener
{
    /**
     * Handle to native window
     */
    protected volatile long hwnd;

    private static final DebugHelper dbg = DebugHelper.create(WComponentPeer.class);
    private static final Logger shapeLog = Logger.getLogger("sun.awt.windows.shape.WComponentPeer");

    static {
        wheelInit();
    }

    // Only actually does stuff if running on 95
    native static void wheelInit();

    // ComponentPeer implementation
    SurfaceData surfaceData;

    private RepaintArea paintArea;

    protected Win32GraphicsConfig winGraphicsConfig;

    boolean isLayouting = false;
    boolean paintPending = false;
    int	    oldWidth = -1;
    int	    oldHeight = -1;
    private int numBackBuffers = 0;
    private VolatileImage backBuffer = null;
    private BufferCapabilities backBufferCaps = null;

    // foreground, background and color are cached to avoid calling back
    // into the Component.
    private Color foreground;
    private Color background;
    private Font font;

    public native boolean isObscured();
    public boolean canDetermineObscurity() { return true; }

    // DropTarget support

    int nDropTargets;
    long nativeDropTargetContext; // native pointer

    public synchronized native void pShow();
    public synchronized native void hide();
    public synchronized native void enable();
    public synchronized native void disable();

    public long getHWnd() {
        return hwnd;
    }

    /* New 1.1 API */
    public native Point getLocationOnScreen();

    /* New 1.1 API */
    public void setVisible(boolean b) {
	if (b) {
    	    show();
    	} else {
    	    hide();
    	}
    }

    public void show() {
	Dimension s = ((Component)target).getSize();
	oldHeight = s.height;
	oldWidth = s.width;
	pShow();
    }    

    /* New 1.1 API */
    public void setEnabled(boolean b) {
    	if (b) {
	    enable();
	} else {
	    disable();
	}
    }

    public int serialNum = 0;

    private native void reshapeNoCheck(int x, int y, int width, int height);

    /* New 1.1 API */
    public void setBounds(int x, int y, int width, int height, int op) {
        // Should set paintPending before reahape to prevent
        // thread race between paint events
        // Native components do redraw after resize
        paintPending = (width != oldWidth) || (height != oldHeight);

        if ( (op & NO_EMBEDDED_CHECK) != 0 ) {
            reshapeNoCheck(x, y, width, height);
        } else {
            reshape(x, y, width, height);
        }
        if ((width != oldWidth) || (height != oldHeight)) {
            // Only recreate surfaceData if this setBounds is called
            // for a resize; a simple move should not trigger a recreation
            try {
                replaceSurfaceData();
            } catch (InvalidPipeException e) {
                // REMIND : what do we do if our surface creation failed?
            }
            oldWidth = width;
            oldHeight = height;
        }

        serialNum++;
    }

    /*
     * Called from native code (on Toolkit thread) in order to
     * dynamically layout the Container during resizing
     */
    void dynamicallyLayoutContainer() {
        // If we got the WM_SIZING, this must be a Container, right?
        // In fact, it must be the top-level Container.
        if (dbg.on) {
            Container parent = WToolkit.getNativeContainer((Component)target);
            dbg.assertion(parent == null);
        }
        final Container cont = (Container)target;

        WToolkit.executeOnEventHandlerThread(cont, new Runnable() {
            public void run() {
                // Discarding old paint events doesn't seem to be necessary.
                cont.invalidate();
                cont.validate();

                if (surfaceData instanceof D3DWindowSurfaceData ||
                    surfaceData instanceof OGLSurfaceData)
                {
                    // When OGL or D3D is enabled, it is necessary to
                    // replace the SurfaceData for each dynamic layout
                    // request so that the viewport stays in sync
                    // with the window bounds.
                    try {
                        replaceSurfaceData();
                    } catch (InvalidPipeException e) {
                        // REMIND: this is unlikely to occur for OGL, but
                        // what do we do if surface creation fails?
                    }
                }

                // Forcing a paint here doesn't seem to be necessary.
                // paintDamagedAreaImmediately();
            }
        });
    }

    /*
     * Paints any portion of the component that needs updating
     * before the call returns (similar to the Win32 API UpdateWindow)
     */
    void paintDamagedAreaImmediately() {
	// force Windows to send any pending WM_PAINT events so
	// the damage area is updated on the Java side
	updateWindow();
	// make sure paint events are transferred to main event queue
	// for coalescing
	WToolkit.getWToolkit().flushPendingEvents();
	// paint the damaged area
	paintArea.paint(target, shouldClearRectBeforePaint());
    }

    native synchronized void updateWindow();

    public void paint(Graphics g) {
        ((Component)target).paint(g);
    }

    public void repaint(long tm, int x, int y, int width, int height) {
    }

    private static final double BANDING_DIVISOR = 4.0;
    private native int[] createPrintedPixels(int srcX, int srcY,
            int srcW, int srcH,
            int alpha);
    public void print(Graphics g) {

        Component comp = (Component)target;

        // To conserve memory usage, we will band the image.

        int totalW = comp.getWidth();
        int totalH = comp.getHeight();

        int hInc = (int)(totalH / BANDING_DIVISOR);
        if (hInc == 0) {
            hInc = totalH;
        }

        for (int startY = 0; startY < totalH; startY += hInc) {
            int endY = startY + hInc - 1;
            if (endY >= totalH) {
                endY = totalH - 1;
            }
            int h = endY - startY + 1;

            Color bgColor = comp.getBackground();
            int[] pix = createPrintedPixels(0, startY, totalW, h,
                    bgColor == null ? 255 : bgColor.getAlpha());
            if (pix != null) {
                BufferedImage bim = new BufferedImage(totalW, h,
                                              BufferedImage.TYPE_INT_ARGB);
                bim.setRGB(0, 0, totalW, h, pix, 0, totalW);
                g.drawImage(bim, 0, startY, null);
                bim.flush();
            }
        }

        comp.print(g);
    }

    public void coalescePaintEvent(PaintEvent e) {
        Rectangle r = e.getUpdateRect();
        if (!(e instanceof IgnorePaintEvent)) {
            paintArea.add(r, e.getID());
        }

        if (dbg.on) {
	    switch(e.getID()) {
	    case PaintEvent.UPDATE:       
                dbg.println("WCP coalescePaintEvent : UPDATE : add : x = " +
		    r.x + ", y = " + r.y + ", width = " + r.width + ",height = " + r.height);
		return;
	    case PaintEvent.PAINT:
                dbg.println("WCP coalescePaintEvent : PAINT : add : x = " +
                    r.x + ", y = " + r.y + ", width = " + r.width + ",height = " + r.height);
		return;
	    }
	}
    }

    public synchronized native void reshape(int x, int y, int width, int height);

    // returns true if the event has been handled and shouldn't be propagated
    // though handleEvent method chain - e.g. WTextFieldPeer returns true 
    // on handling '\n' to prevent it from being passed to native code
    public boolean handleJavaKeyEvent(KeyEvent e) { return false; }

    native void nativeHandleEvent(AWTEvent e);

    public void handleEvent(AWTEvent e) {
        int id = e.getID();

        if (((Component)target).isEnabled() && (e instanceof KeyEvent) && !((KeyEvent)e).isConsumed())  {
            if (handleJavaKeyEvent((KeyEvent)e)) {
                return;
            }
        }

        switch(id) {
            case PaintEvent.PAINT:
                // Got native painting
                paintPending = false;
                // Fallthrough to next statement
            case PaintEvent.UPDATE:
                // Skip all painting while layouting and all UPDATEs
                // while waiting for native paint
                if (!isLayouting && ! paintPending) {
                    paintArea.paint(target,shouldClearRectBeforePaint());
                }
                return;
            default:
            break;
        }

        // Call the native code
        nativeHandleEvent(e);
    }

    public Dimension getMinimumSize() {
	return ((Component)target).getSize();
    }

    public Dimension getPreferredSize() {
	return getMinimumSize();
    }

    // Do nothing for heavyweight implementation
    public void layout() {}

    public Rectangle getBounds() {
    	return ((Component)target).getBounds();
    }

    public boolean isFocusable() {
	return false;
    }

    /*
     * Return the GraphicsConfiguration associated with this peer, either
     * the locally stored winGraphicsConfig, or that of the target Component.
     */
    public GraphicsConfiguration getGraphicsConfiguration() {
        if (winGraphicsConfig != null) {
            return winGraphicsConfig;
        }
        else {
            // we don't need a treelock here, since
            // Component.getGraphicsConfiguration() gets it itself.
            return ((Component)target).getGraphicsConfiguration();
        }
    }

    public SurfaceData getSurfaceData() {
	return surfaceData;
    }

    /**
     * Creates new surfaceData object and invalidates the previous
     * surfaceData object.
     * Replacing the surface data should never lock on any resources which are
     * required by other threads which may have them and may require
     * the tree-lock.
     * This is a degenerate version of replaceSurfaceData(numBackBuffers), so
     * just call that version with our current numBackBuffers.
     */
    public void replaceSurfaceData() {
        replaceSurfaceData(this.numBackBuffers, this.backBufferCaps);
    }

    /**
     * Multi-buffer version of replaceSurfaceData.  This version is called
     * by createBuffers(), which needs to acquire the same locks in the same
     * order, but also needs to perform additional functions inside the
     * locks.
     */
    public void replaceSurfaceData(int newNumBackBuffers,
                                   BufferCapabilities caps)
    {
        SurfaceData oldData = null;
        VolatileImage oldBB = null;
        synchronized(((Component)target).getTreeLock()) {
            synchronized(this) {
                if (pData == 0) {
                    return;
                }
		numBackBuffers = newNumBackBuffers;
                Win32GraphicsConfig gc =
                        (Win32GraphicsConfig)getGraphicsConfiguration();
                ScreenUpdateManager mgr = ScreenUpdateManager.getInstance();
                oldData = surfaceData;
                mgr.dropScreenSurface(oldData);
                surfaceData =
                    mgr.createScreenSurface(gc, this, numBackBuffers, true);
                if (oldData != null) {
                    oldData.invalidate();
                }

                oldBB = backBuffer;
                if (numBackBuffers > 0) {
                    // set the caps first, they're used when creating the bb
                    backBufferCaps = caps;
                    backBuffer = gc.createBackBuffer(this);
                } else if (backBuffer != null) {
                    backBufferCaps = null;
                    backBuffer = null;
                }
            }
        }
        // it would be better to do this before we create new ones,
        // but then we'd run into deadlock issues
        if (oldData != null) {
            oldData.flush();
            // null out the old data to make it collected faster
            oldData = null;
        }
        if (oldBB != null) {
            oldBB.flush();
            // null out the old data to make it collected faster
            oldData = null;
        }
    }

    public void replaceSurfaceDataLater() {
        Runnable r = new Runnable() {
            public void run() {
                // Shouldn't do anything if object is disposed in meanwhile
                // No need for sync as disposeAction in Window is performed 
                // on EDT
                if (!isDisposed()) { 
                    try {
                        replaceSurfaceData();
                    } catch (InvalidPipeException e) {
                    // REMIND : what do we do if our surface creation failed?
                    }
                }
            }
        };
        // Fix 6255371.
        if (!PaintEventDispatcher.getPaintEventDispatcher().queueSurfaceDataReplacing((Component)target, r)) {
            postEvent(new InvocationEvent(Toolkit.getDefaultToolkit(), r));
        }
    }

    /**
     * From the DisplayChangedListener interface.
     *
     * Called after a change in the display mode.  This event
     * triggers replacing the surfaceData object (since that object
     * reflects the current display depth information, which has
     * just changed).
     */
    public void displayChanged() {
        try {
            replaceSurfaceData();
        } catch (InvalidPipeException e) {
            // REMIND : what do we do if our surface creation failed?
        }
    }
    
    /**
     * Part of the DisplayChangedListener interface: components
     * do not need to react to this event
     */
    public void paletteChanged() {
    }

    //This will return null for Components not yet added to a Container
    public ColorModel getColorModel() {
        GraphicsConfiguration gc = getGraphicsConfiguration();
        if (gc != null) {
            return gc.getColorModel();
        }
        else {
            return null;
        }
    }

    //This will return null for Components not yet added to a Container
    public ColorModel getDeviceColorModel() {
        Win32GraphicsConfig gc = 
	    (Win32GraphicsConfig)getGraphicsConfiguration();
        if (gc != null) {
            return gc.getDeviceColorModel();
        }
        else {
            return null;
        }
    }

    //Returns null for Components not yet added to a Container
    public ColorModel getColorModel(int transparency) {
//	return WToolkit.config.getColorModel(transparency);
        GraphicsConfiguration gc = getGraphicsConfiguration();
        if (gc != null) {
            return gc.getColorModel(transparency);
        }
        else {
            return null;
        }
    }
    public java.awt.Toolkit getToolkit() {
	return Toolkit.getDefaultToolkit();
    }

    // fallback default font object
    final static Font defaultFont = new Font(Font.DIALOG, Font.PLAIN, 12);

    public Graphics getGraphics() {
        if (isDisposed()) {
            return null;
        }

        Component target = (Component)getTarget();
        Window window = SunToolkit.getContainingWindow(target);
        if (window != null &&
                !AWTAccessor.getWindowAccessor().isOpaque(window))
        {
            // Non-opaque windows do not support heavyweight children.
            // Redirect all painting to the Window's Graphics instead.
            // The caller is responsible for calling the
            // WindowPeer.updateWindow() after painting has finished.
            int x = 0, y = 0;
            for (Component c = target; c != window; c = c.getParent()) {
                x += c.getX();
                y += c.getY();
            }

            Graphics g =
                ((WWindowPeer)window.getPeer()).getTranslucentGraphics();

            g.translate(x, y);
            g.clipRect(0, 0, target.getWidth(), target.getHeight());

            return g;
        }
        SurfaceData surfaceData = this.surfaceData;
	if (surfaceData != null) {
            /* Fix for bug 4746122. Color and Font shouldn't be null */
            Color bgColor = background;
            if (bgColor == null) {
                bgColor = SystemColor.window;
            }
            Color fgColor = foreground;
            if (fgColor == null) {
                fgColor = SystemColor.windowText;
            }
            Font font = this.font; 
            if (font == null) {
                font = defaultFont;
            }
            ScreenUpdateManager mgr =
                ScreenUpdateManager.getInstance();
            return mgr.createGraphics(surfaceData, this, fgColor,
                                      bgColor, font);
	}
	return null;
    }
    public FontMetrics getFontMetrics(Font font) {
	return WFontMetrics.getFontMetrics(font);
    }

    private synchronized native void _dispose();
    protected void disposeImpl() {
        SurfaceData oldData = surfaceData;
        surfaceData = null;
        ScreenUpdateManager.getInstance().dropScreenSurface(oldData);
        oldData.invalidate();
        // remove from updater before calling targetDisposedPeer
	WToolkit.targetDisposedPeer(target, this);
	_dispose();
    }

    public synchronized void setForeground(Color c) {
        foreground = c;
        _setForeground(c.getRGB());
    }

    public synchronized void setBackground(Color c) {
        background = c;
        _setBackground(c.getRGB());
    }

    /**
     * This method is intentionally not synchronized as it is called while
     * holding other locks.
     *
     * @see sun.java2d.d3d.D3DScreenUpdateManager#validate(D3DWindowSurfaceData)
     */
    public Color getBackgroundNoSync() {
        return background;
    }

    public native void _setForeground(int rgb);
    public native void _setBackground(int rgb);

    public synchronized void setFont(Font f) {
        font = f;
        _setFont(f);
    }
    public synchronized native void _setFont(Font f);
    public final void updateCursorImmediately() {
	WGlobalCursorManager.getCursorManager().updateCursorImmediately();
    }
    
    native static boolean processSynchronousLightweightTransfer(Component heavyweight, Component descendant,
                                                                boolean temporary, boolean focusedWindowChangeAllowed,
                                                                long time);
    public boolean requestFocus
        (Component lightweightChild, boolean temporary,
         boolean focusedWindowChangeAllowed, long time, CausedFocusEvent.Cause cause) {
        if (processSynchronousLightweightTransfer((Component)target, lightweightChild, temporary, 
                                                                      focusedWindowChangeAllowed, time)) {
            return true;
        } else {
            return _requestFocus(lightweightChild, temporary, focusedWindowChangeAllowed, time, cause);
        }
    }
    public native boolean _requestFocus
        (Component lightweightChild, boolean temporary,
         boolean focusedWindowChangeAllowed, long time, CausedFocusEvent.Cause cause);

    public Image createImage(ImageProducer producer) {
	return new ToolkitImage(producer);
    }

    public Image createImage(int width, int height) {
        Win32GraphicsConfig gc =
            (Win32GraphicsConfig)getGraphicsConfiguration();
        return gc.createAcceleratedImage((Component)target, width, height);
    }

    public VolatileImage createVolatileImage(int width, int height) {
        return new SunVolatileImage((Component)target, width, height);
    }

    public boolean prepareImage(Image img, int w, int h, ImageObserver o) {
	return getToolkit().prepareImage(img, w, h, o);
    }

    public int checkImage(Image img, int w, int h, ImageObserver o) {
	return getToolkit().checkImage(img, w, h, o);
    }

    // Object overrides

    public String toString() {
	return getClass().getName() + "[" + target + "]";
    }

    // Toolkit & peer internals

    private int updateX1, updateY1, updateX2, updateY2;

    WComponentPeer(Component target) {
	this.target = target;
        this.paintArea = new RepaintArea();
	Container parent = WToolkit.getNativeContainer(target);
	WComponentPeer parentPeer = (WComponentPeer) WToolkit.targetToPeer(parent);
	create(parentPeer);
        // fix for 5088782: check if window object is created successfully
        checkCreation();
        this.winGraphicsConfig =
            (Win32GraphicsConfig)getGraphicsConfiguration();
        ScreenUpdateManager mgr = ScreenUpdateManager.getInstance();
        this.surfaceData = mgr.createScreenSurface(winGraphicsConfig, this,
                                                   numBackBuffers, false);
	initialize();
	start();  // Initialize enable/disable state, turn on callbacks
    }
    abstract void create(WComponentPeer parent);

    protected void checkCreation()
    {
        if ((hwnd == 0) || (pData == 0))
        {
            if (createError != null)
            {
                throw createError;
            }
            else
            {
                throw new InternalError("couldn't create component peer");
            }
        }
    }

    synchronized native void start();

    void initialize() {
	if (((Component)target).isVisible()) {
	    show();  // the wnd starts hidden
	}
	Color fg = ((Component)target).getForeground();
	if (fg != null) {
	    setForeground(fg);
	}
        // Set background color in C++, to avoid inheriting a parent's color.
	Font  f = ((Component)target).getFont();
	if (f != null) {
	    setFont(f);
	}
	if (! ((Component)target).isEnabled()) {
	    disable();
	}
	Rectangle r = ((Component)target).getBounds();
	setBounds(r.x, r.y, r.width, r.height, SET_BOUNDS);
    }

    // Callbacks for window-system events to the frame

    // Invoke a update() method call on the target
    void handleRepaint(int x, int y, int w, int h) {
        // Repaints are posted from updateClient now...
    }

    // Invoke a paint() method call on the target, after clearing the
    // damaged area.
    void handleExpose(int x, int y, int w, int h) {
        // Bug ID 4081126 & 4129709 - can't do the clearRect() here,
        // since it interferes with the java thread working in the
        // same window on multi-processor NT machines.

        postPaintIfNecessary(x, y, w, h);
    }

    /* Invoke a paint() method call on the target, without clearing the
     * damaged area.  This is normally called by a native control after
     * it has painted itself. 
     *
     * NOTE: This is called on the privileged toolkit thread. Do not
     *       call directly into user code using this thread!
     */
    public void handlePaint(int x, int y, int w, int h) {
        postPaintIfNecessary(x, y, w, h);
    }

    private void postPaintIfNecessary(int x, int y, int w, int h) {
        if (!((Component)target).getIgnoreRepaint()) {
            PaintEvent event = PaintEventDispatcher.getPaintEventDispatcher().
                createPaintEvent((Component)target, x, y, w, h);
            if (event != null) {
                postEvent(event);
            }
	}
    }

    /*
     * Post an event. Queue it for execution by the callback thread.
     */
    void postEvent(AWTEvent event) {
        WToolkit.postEvent(WToolkit.targetToAppContext(target), event);
    }

    // Routines to support deferred window positioning.
    public void beginLayout() {
	// Skip all painting till endLayout
	isLayouting = true;
    }

    public void endLayout() {
        if(!paintArea.isEmpty() && !paintPending &&
            !((Component)target).getIgnoreRepaint()) {
	    // if not waiting for native painting repaint damaged area
	    postEvent(new PaintEvent((Component)target, PaintEvent.PAINT, 
			  new Rectangle()));
	}
	isLayouting = false;
    }		     
		     	
    public native void beginValidate();
    public native void endValidate();

    /**
     * DEPRECATED
     */
    public Dimension minimumSize() {
	return getMinimumSize();
    }

    /**
     * DEPRECATED
     */
    public Dimension preferredSize() {
	return getPreferredSize();
    }

    /**
     * register a DropTarget with this native peer
     */

    public synchronized void addDropTarget(DropTarget dt) {
	if (nDropTargets == 0) {
	    nativeDropTargetContext = addNativeDropTarget();
	}
	nDropTargets++;
    }

    /**
     * unregister a DropTarget with this native peer
     */

    public synchronized void removeDropTarget(DropTarget dt) {
        nDropTargets--;
	if (nDropTargets == 0) {
	    removeNativeDropTarget();
	    nativeDropTargetContext = 0;
	}
    }

    /**
     * add the native peer's AwtDropTarget COM object
     * @return reference to AwtDropTarget object
     */

    native long addNativeDropTarget();

    /**
     * remove the native peer's AwtDropTarget COM object
     */

    native void removeNativeDropTarget();
    native boolean nativeHandlesWheelScrolling();

    public boolean handlesWheelScrolling() {
        // should this be cached?
        return nativeHandlesWheelScrolling();
    }  

    // Returns true if we are inside begin/endLayout and
    // are waiting for native painting    
    public boolean isPaintPending() {
	return paintPending && isLayouting;
    }

    /**
     * The following multibuffering-related methods delegate to our
     * associated GraphicsConfig (Win or WGL) to handle the appropriate
     * native windowing system specific actions.
     */
    
    public void createBuffers(int numBuffers, BufferCapabilities caps)
        throws AWTException
    {
        Win32GraphicsConfig gc =
            (Win32GraphicsConfig)getGraphicsConfiguration();
        gc.assertOperationSupported((Component)target, numBuffers, caps);

        // Re-create the primary surface with the new number of back buffers
        try {
            replaceSurfaceData(numBuffers - 1, caps);
        } catch (InvalidPipeException e) {
            throw new AWTException(e.getMessage());
        }
    }

    public void destroyBuffers() {
        replaceSurfaceData(0, null);
    }

    public void flip(int x1, int y1, int x2, int y2,
                                  BufferCapabilities.FlipContents flipAction)
    {
        VolatileImage backBuffer = this.backBuffer;
        if (backBuffer == null) {
            throw new IllegalStateException("Buffers have not been created");
        }
        Win32GraphicsConfig gc =
            (Win32GraphicsConfig)getGraphicsConfiguration();
        gc.flip(this, (Component)target, backBuffer, x1, y1, x2, y2, flipAction);
    }

    public synchronized Image getBackBuffer() {
        Image backBuffer = this.backBuffer;
        if (backBuffer == null) {
            throw new IllegalStateException("Buffers have not been created");
        }
        return backBuffer;
    }
    public BufferCapabilities getBackBufferCaps() {
        return backBufferCaps;
    }
    public int getBackBuffersNum() {
        return numBackBuffers;
    }

    /* override and return false on components that DO NOT require
       a clearRect() before painting (i.e. native components) */
    public boolean shouldClearRectBeforePaint() {
        return true;
    }

    native void pSetParent(ComponentPeer newNativeParent);

    /**
     * @see java.awt.peer.ComponentPeer#reparent
     */
    public void reparent(ContainerPeer newNativeParent) {        
        pSetParent(newNativeParent);
    }

    /**
     * @see java.awt.peer.ComponentPeer#isReparentSupported
     */
    public boolean isReparentSupported() {
        return true;
    }

    public void setBoundsOperation(int operation) {
    }

    private volatile boolean isAccelCapable = true;
    // REMIND: Temp workaround for issues with using HW acceleration
    // in the browser on Vista when DWM is enabled.
    // @return true if the toplevel container is not an EmbeddedFrame or
    // if this EmbeddedFrame is acceleration capable, false otherwise
    private static final boolean isContainingTopLevelAccelCapable(Component c) {
        while (c != null && !(c instanceof WEmbeddedFrame)) {
            c = c.getParent();
        }
        if (c == null) {
            return true;
        }
        return ((WEmbeddedFramePeer)c.getPeer()).isAccelCapable();
    }
    /**
     * Returns whether this component is capable of being hw accelerated.
     * More specifically, whether rendering to this component or a
     * BufferStrategy's back-buffer for this component can be hw accelerated.
     *
     * Conditions which could prevent hw acceleration include the toplevel
     * window containing this component being
     * {@link com.sun.awt.AWTUtilities.Translucency#TRANSLUCENT TRANSLUCENT}.
     *
     * Another condition is if Xor paint mode was detected when rendering
     * to an on-screen accelerated surface associated with this peer.
     * In this case both on- and off-screen acceleration for this peer is
     * disabled.
     *
     * @return {@code true} if this component is capable of being hw
     * accelerated, {@code false} otherwise
     * @see com.sun.awt.AWTUtilities.Translucency#TRANSLUCENT
     */
    public boolean isAccelCapable() {
        // REMIND: Temp workaround for issues with using HW acceleration
        // in the browser on Vista when DWM is enabled
        if (!isAccelCapable ||
            !isContainingTopLevelAccelCapable((Component)target))
        {
            return false;
        }

        boolean isTranslucent =
            SunToolkit.isContainingTopLevelTranslucent((Component)target);
        // D3D/OGL and translucent windows interacted poorly in Windows XP;
        // these problems are no longer present in Vista
        return !isTranslucent || Win32GraphicsEnvironment.isVistaOS();
    }

    /**
     * Disables acceleration for this peer.
     */
    public void disableAcceleration() {
        isAccelCapable = false;
    }

    private native void setRectangularShape(int lox, int loy, int hix, int hiy,
                     Region region);

    
    /**
     * Applies the shape to the native component window.
     */
    public void applyShape(Region shape) {
        if (shapeLog.isLoggable(Level.FINER)) {
            shapeLog.finer(
                    "*** INFO: Setting shape: PEER: " + this
                    + "; TARGET: " + target
                    + "; SHAPE: " + shape);
        }

        if (shape != null) {
            setRectangularShape(shape.getLoX(), shape.getLoY(), shape.getHiX(), shape.getHiY(), 
                    (shape.isRectangular() ? null : shape));
        } else {
            setRectangularShape(0, 0, 0, 0, null);
        }
    }
}

