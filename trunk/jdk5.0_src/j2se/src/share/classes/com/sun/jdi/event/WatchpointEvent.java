/*
 * @(#)WatchpointEvent.java	1.14 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.jdi.event;

import com.sun.jdi.*;

/**
 * Notification of a field triggered event encountered by a thread in the 
 * target VM.
 *
 * @see EventQueue
 * @see VirtualMachine
 *
 * @author Robert Field
 * @since  1.3
 */
public interface WatchpointEvent extends LocatableEvent {

    /**
     * Returns the field that is about to be accessed/modified. 
     *
     * @return a {@link Field} which mirrors the field
     * in the target VM.
     * @throws ObjectCollectedException may be thrown if class 
     * has been garbage collected.
     */
    Field field();

    /**
     * Returns the object whose field is about to be accessed/modified. 
     * Return null is the access is to a static field.
     *
     * @return a {@link ObjectReference} which mirrors the event's 
     * object in the target VM.
     */
    ObjectReference object();

    /**
     * Current value of the field.
     * @throws ObjectCollectedException if object or class have been 
     * garbage collected.
     */
    Value valueCurrent();
}
    

