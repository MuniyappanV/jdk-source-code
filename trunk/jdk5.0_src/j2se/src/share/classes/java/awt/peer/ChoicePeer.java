/*
 * @(#)ChoicePeer.java	1.17 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package java.awt.peer;

import java.awt.*;

/**
 * The peer interfaces are intended only for use in porting
 * the AWT. They are not intended for use by application
 * developers, and developers should not implement peers
 * nor invoke any of the peer methods directly on the peer
 * instances.
 */
public interface ChoicePeer extends ComponentPeer {
    void add(String item, int index);
    void remove(int index);
    void removeAll();
    void select(int index);

    /*
     * DEPRECATED:  Replaced by add(String, int).
     */
    void addItem(String item, int index);
}
