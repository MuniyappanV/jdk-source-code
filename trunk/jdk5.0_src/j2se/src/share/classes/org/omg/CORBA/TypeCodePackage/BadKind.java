/*
 * @(#)BadKind.java	1.19 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package org.omg.CORBA.TypeCodePackage;

/**
 * The exception <code>BadKind</code> is thrown when 
 * an inappropriate operation is invoked on a <code>TypeCode</code> object. For example,
 * invoking the method <code>discriminator_type()</code> on an instance of
 * <code>TypeCode</code> that does not represent an IDL union will cause the
 * exception <code>BadKind</code> to be thrown.
 *
 * @see org.omg.CORBA.TypeCode
 * @version 1.7, 03/18/98
 * @since   JDK1.2
 */

public final class BadKind extends org.omg.CORBA.UserException {
    /**
     * Constructs a <code>BadKind</code> exception with no reason message.
     */
    public BadKind() {
	super();
    }

    /**
     * Constructs a <code>BadKind</code> exception with the specified 
     * reason message.
     * @param reason the String containing a reason message
     */
    public BadKind(String reason) {
	super(reason);
    }
}
