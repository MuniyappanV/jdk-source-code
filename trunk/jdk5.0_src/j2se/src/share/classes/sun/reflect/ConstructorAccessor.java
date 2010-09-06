/*
 * @(#)ConstructorAccessor.java	1.7 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.reflect;

import java.lang.reflect.InvocationTargetException;

/** This interface provides the declaration for
    java.lang.reflect.Constructor.invoke(). Each Constructor object is
    configured with a (possibly dynamically-generated) class which
    implements this interface. */

public interface ConstructorAccessor {
    /** Matches specification in {@link java.lang.reflect.Constructor} */
    public Object newInstance(Object[] args)
        throws InstantiationException,
               IllegalArgumentException,
               InvocationTargetException;
}
