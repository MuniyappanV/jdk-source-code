/*
 * @(#)TypeVariable.java	1.4 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 * 
 */

package com.sun.javadoc;


/**
 * Represents a type variable.
 * For example, the generic interface {@code List<E>} has a single
 * type variable {@code E}.
 * A type variable may have explicit bounds, as in
 * {@code C<R extends Remote>}.
 *
 * @author Scott Seligman
 * @version 1.4 10/03/23
 * @since 1.5
 */
public interface TypeVariable extends Type {

    /**
     * Return the bounds of this type variable.
     * These are the types given by the <i>extends</i> clause.
     * Return an empty array if there are no explicit bounds.
     *
     * @return the bounds of this type variable.
     */
    Type[] bounds();

    /**
     * Return the class, interface, method, or constructor within
     * which this type variable is declared.
     *
     * @return the class, interface, method, or constructor within
     *         which this type variable is declared.
     */
    ProgramElementDoc owner();
}