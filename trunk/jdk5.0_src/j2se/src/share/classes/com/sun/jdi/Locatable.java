/*
 * @(#)Locatable.java	1.17 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.jdi;

/**
 * A mirror that has a {@link Location}. 
 *
 * @author Robert Field
 * @author Gordon Hirsch
 * @author James McIlree
 * @since  1.3
 */
public interface Locatable {
    /**
     * Returns the {@link Location} of this mirror, if there
     * is executable code associated with it. Note that both
     * Java<SUP><FONT SIZE="-2">TM</FONT></SUP> programming
     * language methods and native methods have executable code.
     * Returns null for abstract methods, since abstract methods
     * have no executable code.
     * 
     * @return the {@link Location} of this mirror, or null if 
     * there is no executable code associated with it. 
     */
    Location location();
}
