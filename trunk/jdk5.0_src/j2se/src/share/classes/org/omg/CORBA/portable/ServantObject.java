/*
 * @(#)ServantObject.java	1.14 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */


package org.omg.CORBA.portable;

/**
 This class is part of the local stub API, the purpose of which is to provide
 high performance calls for collocated clients and servers
 (i.e. clients and servers residing in the same Java VM).
 The local stub API is supported via three additional methods on
 <code>ObjectImpl</code> and <code>Delegate</code>.
 ORB vendors may subclass this class to return additional
 request state that may be required by their implementations.
 @see ObjectImpl
 @see Delegate
*/

public class ServantObject
{
    /** The real servant. The local stub may cast this field to the expected type, and then
     * invoke the operation directly. Note, the object may or may not be the actual servant
     * instance. 
     * @return The real servant
     */
    public java.lang.Object servant;
}
