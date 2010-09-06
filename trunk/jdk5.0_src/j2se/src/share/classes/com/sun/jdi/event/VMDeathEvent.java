/*
 * @(#)VMDeathEvent.java	1.16 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.jdi.event;

import com.sun.jdi.*;

/**
 * Notification of target VM termination.
 * This event occurs if the target VM terminates before the 
 * VM disconnects ({@link VMDisconnectEvent}).
 * Thus, this event will NOT occur if 
 * external forces terminate the connection (e.g. a crash)
 * or if the connection is intentionally terminated with
 * {@link com.sun.jdi.VirtualMachine#dispose()
 *      VirtualMachine.dispose()}
 * <P>
 * On VM termination, a single unsolicited VMDeathEvent
 * will always be sent with a 
 * {@link com.sun.jdi.request.EventRequest#suspendPolicy() suspend policy} 
 * of {@link com.sun.jdi.request.EventRequest#SUSPEND_NONE SUSPEND_NONE}.
 * Additional VMDeathEvents will be sent in the same event set if they are
 * requested with a
 * {@link com.sun.jdi.request.VMDeathRequest VMDeathRequest}.
 * <P>
 * The VM is still intact and can be queried at the point this
 * event was initiated but immediately thereafter it is not
 * considered intact and cannot be queried. 
 * Note: If the enclosing {@link EventSet} has a
 * {@link com.sun.jdi.request.EventRequest#suspendPolicy() suspend policy} 
 * other than
 * {@link com.sun.jdi.request.EventRequest#SUSPEND_ALL SUSPEND_ALL}
 * the initiating point may be long past. 
 * <P>
 * All VMDeathEvents will be in a single {@link EventSet},
 * no other events will be in the event set.  A resume
 * must occur to continue execution after any event set which
 * performs suspensions - in this case to allow proper shutdown.
 *
 * @see VMDisconnectEvent
 * @see com.sun.jdi.request.EventRequestManager#createVMDeathRequest
 * @see com.sun.jdi.request.VMDeathRequest
 * @see EventQueue
 * @see VirtualMachine
 *
 * @author Robert Field
 * @since  1.3
 */
public interface VMDeathEvent extends Event {
}

