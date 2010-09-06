/*
 * @(#)InternalEventHandler.java	1.18 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.tools.jdi;

import com.sun.jdi.*;
import com.sun.jdi.event.*;
import java.util.*;

public class InternalEventHandler implements Runnable
{
    EventQueueImpl queue;
    VirtualMachineImpl vm;

    InternalEventHandler(VirtualMachineImpl vm, EventQueueImpl queue)
    {
        this.vm = vm;
        this.queue = queue;
        Thread thread = new Thread(vm.threadGroupForJDI(), this,
				   "JDI Internal Event Handler");
        thread.setDaemon(true);
        thread.start();
    }

    public void run() {
        if ((vm.traceFlags & VirtualMachine.TRACE_EVENTS) != 0) {
            vm.printTrace("Internal event handler running");
        }
        try {
            while (true) {
                try {
                    EventSet eventSet = queue.removeInternal();
                    EventIterator it = eventSet.eventIterator();
                    while (it.hasNext()) {
                        Event event = it.nextEvent();
                        if (event instanceof ClassUnloadEvent) {
                            ClassUnloadEvent cuEvent = (ClassUnloadEvent)event;
                            vm.removeReferenceType(cuEvent.classSignature());
    
                            if ((vm.traceFlags & vm.TRACE_EVENTS) != 0) {
                                vm.printTrace("Handled Unload Event for " + 
                                              cuEvent.classSignature());
                            }
                        } else if (event instanceof ClassPrepareEvent) {
                            ClassPrepareEvent cpEvent = (ClassPrepareEvent)event;
                            ((ReferenceTypeImpl)cpEvent.referenceType())
                                                            .markPrepared();
    
                            if ((vm.traceFlags & vm.TRACE_EVENTS) != 0) {
                                vm.printTrace("Handled Prepare Event for " + 
                                              cpEvent.referenceType().name());
                            }
                        }
    
                    }

                /*
                 * Handle exceptions that can occur in normal operation
                 * but which can't be accounted for by event builder
                 * methods. The thread should not be terminated if they
                 * occur.  
                 *
                 * TO DO: We need a better way to log these conditions. 
                 */
                } catch (VMOutOfMemoryException vmme) {
                    vmme.printStackTrace();
                } catch (InconsistentDebugInfoException idie) {
                    idie.printStackTrace();

                /*
                 * If any of these exceptions below occurs, there is some
                 * sort of programming error that should be addressed in 
                 * the JDI implemementation. However, it would cripple
                 * the implementation if we let this thread die due to 
                 * one of them. So, a notification of the exception is
                 * given and we attempt to continue. 
                 */
                } catch (ObjectCollectedException oce) {
                    oce.printStackTrace();
                } catch (ClassNotPreparedException cnpe) {
                    cnpe.printStackTrace();
                }
            }
        } catch (InterruptedException e) {  // should we really die here
        } catch (VMDisconnectedException e) {  // time to die
        }
        if ((vm.traceFlags & VirtualMachine.TRACE_EVENTS) != 0) {
            vm.printTrace("Internal event handler exiting");
        }
    }
}

