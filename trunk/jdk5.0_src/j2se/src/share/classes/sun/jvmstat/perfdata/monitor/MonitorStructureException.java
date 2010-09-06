/*
 * @(#)MonitorStructureException.java	1.1 04/02/23
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.jvmstat.perfdata.monitor;

import sun.jvmstat.monitor.MonitorException;

/**
 * Exception indicating that improperly formatted data was encountered
 * while parsing a HotSpot PerfData buffer.
 *
 * @author Brian Doherty
 * @version 1.1, 02/23/04
 * @since 1.5
 */
public class MonitorStructureException extends MonitorException {

    /**
     * Constructs a <code>MonitorStructureException</code> with <code>
     * null</code> as its error detail message.
     */
     public MonitorStructureException() {
         super();
     }

    /**
     * Constructs an <code>MonitorStructureException</code> with the specified
     * detail message. The error message string <code>s</code> can later be
     * retrieved by the <code>{@link java.lang.Throwable#getMessage}</code>
     * method of class <code>java.lang.Throwable</code>.
     *
     * @param s the detail message.
     */
    public MonitorStructureException(String s) {
        super(s);
    }
}
