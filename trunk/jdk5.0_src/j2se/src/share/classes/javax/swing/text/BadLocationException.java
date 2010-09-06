/*
 * @(#)BadLocationException.java	1.20 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package javax.swing.text;

/**
 * This exception is to report bad locations within a document model
 * (that is, attempts to reference a location that doesn't exist).
 * <p>
 * <strong>Warning:</strong>
 * Serialized objects of this class will not be compatible with
 * future Swing releases. The current serialization support is
 * appropriate for short term storage or RMI between applications running
 * the same version of Swing.  As of 1.4, support for long term storage
 * of all JavaBeans<sup><font size="-2">TM</font></sup>
 * has been added to the <code>java.beans</code> package.
 * Please see {@link java.beans.XMLEncoder}.
 *
 * @author  Timothy Prinzing
 * @version 1.20 12/19/03
 */
public class BadLocationException extends Exception
{
    /**
     * Creates a new BadLocationException object.
     * 
     * @param s		a string indicating what was wrong with the arguments
     * @param offs      offset within the document that was requested >= 0
     */
    public BadLocationException(String s, int offs) {
	super(s);
	this.offs = offs;
    }

    /**
     * Returns the offset into the document that was not legal.
     *
     * @return the offset >= 0
     */
    public int offsetRequested() {
	return offs;
    }

    private int offs;
}
