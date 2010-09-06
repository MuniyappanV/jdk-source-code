/*
 * @(#)FocusListener.java	1.15 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.awt.event;

import java.util.EventListener;

/**
 * The listener interface for receiving keyboard focus events on 
 * a component.
 * The class that is interested in processing a focus event
 * either implements this interface (and all the methods it
 * contains) or extends the abstract <code>FocusAdapter</code> class
 * (overriding only the methods of interest).
 * The listener object created from that class is then registered with a
 * component using the component's <code>addFocusListener</code> 
 * method. When the component gains or loses the keyboard focus,
 * the relevant method in the listener object 
 * is invoked, and the <code>FocusEvent</code> is passed to it.
 *
 * @see FocusAdapter
 * @see FocusEvent
 * @see <a href="http://java.sun.com/docs/books/tutorial/post1.0/ui/focuslistener.html">Tutorial: Writing a Focus Listener</a>
 * @see <a href="http://www.awl.com/cp/javaseries/jcl1_2.html">Reference: The Java Class Libraries (update file)</a>
 *
 * @author Carl Quinn
 * @version 1.15 12/19/03
 * @since 1.1
 */
public interface FocusListener extends EventListener {

    /**
     * Invoked when a component gains the keyboard focus.
     */
    public void focusGained(FocusEvent e);

    /**
     * Invoked when a component loses the keyboard focus.
     */
    public void focusLost(FocusEvent e);
}
