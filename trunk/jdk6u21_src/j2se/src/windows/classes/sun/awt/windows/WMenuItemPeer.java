/*
 * @(#)WMenuItemPeer.java	1.41 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package sun.awt.windows;

import java.util.ResourceBundle;
import java.util.MissingResourceException;
import java.awt.*;
import java.awt.peer.*;
import java.awt.event.ActionEvent;
import java.security.AccessController;
import java.security.PrivilegedAction;
import sun.awt.AppContext;

class WMenuItemPeer extends WObjectPeer implements MenuItemPeer {

    static {
        initIDs();
    }

    String shortcutLabel;
    //WMenuBarPeer extends WMenuPeer
    //so parent is always instanceof WMenuPeer
    protected WMenuPeer parent;

    // MenuItemPeer implementation

    private synchronized native void _dispose();
    protected void disposeImpl() {
        WToolkit.targetDisposedPeer(target, this);
	_dispose();
    }

    public void setEnabled(boolean b) {
	enable(b);
    }

    /**
     * DEPRECATED:  Replaced by setEnabled(boolean).
     */
    public void enable() {
        enable(true);
    }

    /**
     * DEPRECATED:  Replaced by setEnabled(boolean).
     */
    public void disable() {
        enable(false);
    }

    public void readShortcutLabel() {
        //Fix for 6288578: PIT. Windows: Shortcuts displayed for the menuitems in a popup menu 
        WMenuPeer ancestor = parent;
        while (ancestor != null && !(ancestor instanceof WMenuBarPeer)) {
            ancestor = ancestor.parent;
        }
        if (ancestor instanceof WMenuBarPeer) {
            MenuShortcut sc = ((MenuItem)target).getShortcut();
            shortcutLabel = (sc != null) ? sc.toString() : null;
        } else {
            shortcutLabel = null;
        }
    }

    public void setLabel(String label) {
        //Fix for 6288578: PIT. Windows: Shortcuts displayed for the menuitems in a popup menu 
        readShortcutLabel();
        _setLabel(label);
    }
    public native void _setLabel(String label);

    // Toolkit & peer internals

    boolean	isCheckbox = false;

    protected WMenuItemPeer() {
    }
    WMenuItemPeer(MenuItem target) {
	this.target = target;
	this.parent = (WMenuPeer) WToolkit.targetToPeer(target.getParent());
	create(parent);
        // fix for 5088782: check if menu object is created successfully
        checkMenuCreation();
        //Fix for 6288578: PIT. Windows: Shortcuts displayed for the menuitems in a popup menu 
        readShortcutLabel();
    }

    protected void checkMenuCreation()
    {
        // fix for 5088782: check if menu peer is created successfully
        if (pData == 0)
        {
            if (createError != null)
            {
                throw createError;
            }
            else
            {
                throw new InternalError("couldn't create menu peer");
            }
        }

    }

    /*
     * Post an event. Queue it for execution by the callback thread.
     */
    void postEvent(AWTEvent event) {
        WToolkit.postEvent(WToolkit.targetToAppContext(target), event);
    }

    native void create(WMenuPeer parent);

    native void enable(boolean e);

    // native callbacks

    void handleAction(final long when, final int modifiers) {
	WToolkit.executeOnEventHandlerThread(target, new Runnable() {
	    public void run() {
		postEvent(new ActionEvent(target, ActionEvent.ACTION_PERFORMED,
					  ((MenuItem)target).
                                              getActionCommand(), when,
                                          modifiers));
	    }
	});
    }

    private static Font defaultMenuFont;

    static {
        defaultMenuFont = (Font) AccessController.doPrivileged(
            new PrivilegedAction() {
                public Object run() {
                    try {
                        ResourceBundle rb = ResourceBundle.getBundle("sun.awt.windows.awtLocalization");
                        return Font.decode(rb.getString("menuFont"));
                    } catch (MissingResourceException e) {
                        System.out.println(e.getMessage());
                        System.out.println("Using default MenuItem font\n");
                        return new Font("SanSerif", Font.PLAIN, 11);
                    }
                }
            });
    }

    static Font getDefaultFont() {
        return defaultMenuFont;
    }

    /**
     * Initialize JNI field and method IDs
     */
    private static native void initIDs();

    // Needed for MenuComponentPeer.
    public void setFont(Font f) {
    }
}
