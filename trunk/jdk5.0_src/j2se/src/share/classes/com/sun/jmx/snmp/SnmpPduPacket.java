/*
 * @(#)file      SnmpPduPacket.java
 * @(#)author    Sun Microsystems, Inc.
 * @(#)version   4.14
 * @(#)date      04/10/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 */
// Copyright (c) 1995-96 by Cisco Systems, Inc.

package com.sun.jmx.snmp;


import java.io.Serializable;
import java.net.InetAddress;


/**
 * Is the fully decoded representation of an SNMP packet.
 * <P>
 * You will not usually need to use this class, except if you
 * decide to implement your own 
 * {@link com.sun.jmx.snmp.SnmpPduFactory SnmpPduFactory} object.
 * <P>
 * Classes are derived from <CODE>SnmpPduPacket</CODE> to
 * represent the different forms of SNMP packets
 * ({@link com.sun.jmx.snmp.SnmpPduRequest SnmpPduRequest},
 * {@link com.sun.jmx.snmp.SnmpPduTrap SnmpPduTrap},
 * {@link com.sun.jmx.snmp.SnmpPduBulk SnmpPduBulk}).
 * <BR>The <CODE>SnmpPduPacket</CODE> class defines the attributes 
 * common to every form of SNMP packets.
 * 
 * 
 * <p><b>This API is a Sun Microsystems internal API  and is subject 
 * to change without notice.</b></p>
 * @see SnmpMessage
 * @see SnmpPduFactory
 *
 * @version     4.14     12/19/03
 * @author      Sun Microsystems, Inc
 */

public abstract class SnmpPduPacket extends SnmpPdu implements Serializable {
    /**
     * The pdu community string.
     */
    public byte[] community ;
}

