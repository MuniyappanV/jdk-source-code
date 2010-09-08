/*
 * @(#)WFileDialogPeer.java	1.51 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package sun.awt.windows;

import java.awt.*;
import java.awt.dnd.DropTarget;
import java.awt.peer.*;
import java.io.File;
import java.io.FilenameFilter;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ResourceBundle;
import java.util.MissingResourceException;
import java.util.Vector;
import sun.awt.AppContext;
import sun.awt.*;
import sun.awt.CausedFocusEvent;

public class WFileDialogPeer extends WWindowPeer implements FileDialogPeer {

    static {
        initIDs();
    }

    private WComponentPeer parent;
	private FilenameFilter fileFilter;

    private Vector<WWindowPeer> blockedWindows = new Vector<WWindowPeer>();

    //Needed to fix 4152317
    private static native void setFilterString(String allFilter);

    public void	setFilenameFilter(FilenameFilter filter) {
		this.fileFilter = filter;
    }

    boolean checkFilenameFilter(String filename) {
        FileDialog fileDialog = (FileDialog)target;
        if (fileFilter == null) {
            return true;
        }
        File file = new File(filename);
        return fileFilter.accept(new File(file.getParent()), file.getName());
    }

    // Toolkit & peer internals
    WFileDialogPeer(FileDialog target) {
	super(target);
    }

    void create(WComponentPeer parent) {
        this.parent = parent;
    }

    // don't use checkCreation() from WComponentPeer to avoid hwnd check
    protected void checkCreation() {
    }

    void initialize() {}

    private native void _dispose();
    protected void disposeImpl() {
	WToolkit.targetDisposedPeer(target, this);
	_dispose();
    }

    private native void _show();
    private native void _hide();

    public void show() {
        new Thread(new Runnable() {
            public void run() {
                _show();
            }
        }).start();
    }

    public void hide() {
        _hide();
    }

    // called from native code when the dialog is shown or hidden
    void setHWnd(long hwnd) {
        if (this.hwnd == hwnd) {
            return;
        }
        this.hwnd = hwnd;
        for (WWindowPeer window : blockedWindows) {
            if (hwnd != 0) {
                window.modalDisable((Dialog)target, hwnd);
            } else {
                window.modalEnable((Dialog)target);
            }
        }
    }

    // NOTE: This method is called by privileged threads.
    //       DO NOT INVOKE CLIENT CODE ON THIS THREAD!
    void handleSelected(final String file) {
        final FileDialog fileDialog = (FileDialog)target;
	WToolkit.executeOnEventHandlerThread(fileDialog, new Runnable() {
	    public void run() {
                int index = file.lastIndexOf(java.io.File.separatorChar);/*2509*//*ibm*/
		String dir;

		if (index == -1) {
                    dir = "."+java.io.File.separator;
		    fileDialog.setFile(file);
		}
		else {
		    dir = file.substring(0, index + 1);
		    fileDialog.setFile(file.substring(index + 1));
		}
		fileDialog.setDirectory(dir);
		fileDialog.hide();
	    }
	});
    } // handleSelected()

    // NOTE: This method is called by privileged threads.
    //       DO NOT INVOKE CLIENT CODE ON THIS THREAD!
    void handleCancel() {
        final FileDialog fileDialog = (FileDialog)target;
	WToolkit.executeOnEventHandlerThread(fileDialog, new Runnable() {
	    public void run() {
	        fileDialog.setFile(null);
		fileDialog.hide();
	    }
	});
    } // handleCancel()

    //This whole static block is a part of 4152317 fix
    static {
        String filterString = (String) AccessController.doPrivileged(
            new PrivilegedAction() {
                public Object run() {
                    try {
                        ResourceBundle rb = ResourceBundle.getBundle("sun.awt.windows.awtLocalization");
                        return rb.getString("allFiles");
                    } catch (MissingResourceException e) {
                        return "All Files";
                    }
                }
            });
        setFilterString(filterString);
    }

    void blockWindow(WWindowPeer window) {
        blockedWindows.add(window);
        // if this dialog hasn't got an HWND, notification is
        // postponed until setHWnd() is called
        if (hwnd != 0) {
            window.modalDisable((Dialog)target, hwnd);
        }
    }
    void unblockWindow(WWindowPeer window) {
        blockedWindows.remove(window);
        // if this dialog hasn't got an HWND or has been already
        // closed, don't send notification
        if (hwnd != 0) {
            window.modalEnable((Dialog)target);
        }
    }

    public void blockWindows(java.util.List<Window> toBlock) {
        for (Window w : toBlock) {
            WWindowPeer wp = (WWindowPeer)AWTAccessor.getComponentAccessor().getPeer(w);
            if (wp != null) {
                blockWindow(wp);
            }
        }
    }

    public native void toFront();
    public native void toBack();

    // unused methods.  Overridden to disable this functionality as
    // it requires HWND which is not available for FileDialog
    public void setAlwaysOnTop(boolean value) {}
    public void setDirectory(String dir) {}
    public void setFile(String file) {}
    public void setTitle(String title) {}

    public void setResizable(boolean resizable) {}
    public void enable() {}
    public void disable() {}
    public void reshape(int x, int y, int width, int height) {}
    public boolean handleEvent(Event e) { return false; }
    public void setForeground(Color c) {}
    public void setBackground(Color c) {}
    public void setFont(Font f) {}
    public void updateMinimumSize() {}
    public void updateIconImages() {}
    
    public boolean requestFocus(boolean temporary,
        boolean focusedWindowChangeAllowed) {
        return false;
    }
    
    public boolean requestFocus
        (Component lightweightChild, boolean temporary,
         boolean focusedWindowChangeAllowed, long time, CausedFocusEvent.Cause cause)
    {
         
         return false;
    }             

    void start() {}
    public void beginValidate() {}
    public void endValidate() {}
    void invalidate(int x, int y, int width, int height) {}
    public void addDropTarget(DropTarget dt) {}
    public void removeDropTarget(DropTarget dt) {}
    public void updateFocusableWindowState() {}

    /**
     * Initialize JNI field and method ids
     */
    private static native void initIDs();

    /**
     * WFileDialogPeer doesn't have native pData so we don't do restack on it
     * @see java.awt.peer.ContainerPeer#restack
     */
    public void restack() {
    }    

    /**
     * @see java.awt.peer.ContainerPeer#isRestackSupported
     */
    public boolean isRestackSupported() {
        return false;
    }

    // The effects are not supported for system dialogs.
    public void applyShape(sun.java2d.pipe.Region shape) {}
    public void setOpacity(float opacity) {}
    public void setOpaque(boolean isOpaque) {}
    public void updateWindow(java.awt.image.BufferedImage backBuffer) {}
}
