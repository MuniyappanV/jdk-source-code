/*
 * @(#)StepEvent.java	1.17 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.jdi.event;

import com.sun.jdi.*;

/**
 * Notification of step completion in the target VM. 
 * The step event 
 * is generated immediately before the code at its location is executed;
 * thus, if the step is entering a new method (as might occur with
 * {@link com.sun.jdi.request.StepRequest#STEP_INTO StepRequest.STEP_INTO})
 * the location of the event is the first instruction of the method.
 * When a step leaves a method, the location of the event will be the
 * first instruction after the call in the calling method; note that
 * this location may not be at a line boundary, even if 
 * {@link com.sun.jdi.request.StepRequest#STEP_LINE StepRequest.STEP_LINE}
 * was used.
 *
 * @see com.sun.jdi.request.StepRequest
 * @see EventQueue
 *
 * @author Robert Field
 * @since  1.3
 */
public interface StepEvent extends LocatableEvent {

}
    

