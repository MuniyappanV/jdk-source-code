/*
 * @(#)IllegalMonitorStateException.java	1.12 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.lang;

/**
 * Thrown to indicate that a thread has attempted to wait on an 
 * object's monitor or to notify other threads waiting on an object's
 * monitor without owning the specified monitor. 
 *
 * @author  unascribed
 * @version 1.12, 12/19/03
 * @see     java.lang.Object#notify()
 * @see     java.lang.Object#notifyAll()
 * @see     java.lang.Object#wait() 
 * @see     java.lang.Object#wait(long) 
 * @see     java.lang.Object#wait(long, int) 
 * @since   JDK1.0
 */
public
class IllegalMonitorStateException extends RuntimeException {
    /**
     * Constructs an <code>IllegalMonitorStateException</code> with no 
     * detail message. 
     */
    public IllegalMonitorStateException() {
	super();
    }

    /**
     * Constructs an <code>IllegalMonitorStateException</code> with the 
     * specified detail message. 
     *
     * @param   s   the detail message.
     */
    public IllegalMonitorStateException(String s) {
	super(s);
    }
}
