/*
 * @(#)RemoteHostImpl.java	1.1 04/02/23
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.tools.jstatd;

import java.util.*;
import java.nio.*;
import java.io.*;
import java.net.*;
import java.rmi.*;
import java.rmi.server.*;
import sun.jvmstat.monitor.*;
import sun.jvmstat.monitor.event.*;
import sun.jvmstat.monitor.remote.*;

/**
 * Concrete implementation of the RemoteHost interface for the HotSpot
 * PerfData <em>rmi:</em> protocol.
 * <p>
 * This class provides remote access to the instrumentation exported
 * by HotSpot Java Virtual Machines through the PerfData shared memory
 * interface.
 *
 * @author Brian Doherty
 * @version 1.1, 02/23/04
 * @since 1.5
 */
public class RemoteHostImpl implements RemoteHost, HostListener {

    private MonitoredHost monitoredHost;
    private Set activeVms;

    public RemoteHostImpl() throws MonitorException {
        try {
            monitoredHost = MonitoredHost.getMonitoredHost("localhost");
        } catch (URISyntaxException e) { }

        activeVms = monitoredHost.activeVms();
        monitoredHost.addHostListener(this);
    }

    public RemoteVm attachVm(int lvmid, String mode)
                    throws RemoteException, MonitorException {
        Integer v = new Integer(lvmid);
        RemoteVm stub = null;
        StringBuffer sb = new StringBuffer();

        sb.append("local://").append(lvmid).append("@localhost");
        if (mode != null) {
            sb.append("?mode=" + mode);
        }

        String vmidStr = sb.toString();

        try {
            VmIdentifier vmid = new VmIdentifier(vmidStr);
            MonitoredVm mvm = monitoredHost.getMonitoredVm(vmid);
            RemoteVmImpl rvm = new RemoteVmImpl((BufferedMonitoredVm)mvm);
            stub = (RemoteVm) UnicastRemoteObject.exportObject(rvm, 0);
        }
        catch (URISyntaxException e) {
            throw new RuntimeException("Malformed VmIdentifier URI: "
                                       + vmidStr, e);
        }
        return stub;
    }

    public void detachVm(RemoteVm rvm) throws RemoteException {
        rvm.detach();
    }

    public int[] activeVms() throws MonitorException {
        Object[] vms = null;
        int[] vmids = null;

        vms = monitoredHost.activeVms().toArray();
        vmids = new int[vms.length];

        for (int i = 0; i < vmids.length; i++) {
            vmids[i] = ((Integer)vms[i]).intValue();
        }
        return vmids;
    }

    public void vmStatusChanged(VmStatusChangeEvent ev) {
        synchronized(this.activeVms) {
            activeVms.retainAll(ev.getActive());
        }
    }

    public void disconnected(HostEvent ev) {
        // we only monitor the local host, so this event shouldn't occur.
    }
}
