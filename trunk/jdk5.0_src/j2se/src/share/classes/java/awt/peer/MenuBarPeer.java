/*
 * @(#)MenuBarPeer.java	1.15 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package java.awt.peer;

import java.awt.Menu;

/**
 * The peer interfaces are intended only for use in porting
 * the AWT. They are not intended for use by application
 * developers, and developers should not implement peers
 * nor invoke any of the peer methods directly on the peer
 * instances.
 */
public interface MenuBarPeer extends MenuComponentPeer {
    void addMenu(Menu m);
    void delMenu(int index);
    void addHelpMenu(Menu m);
}
