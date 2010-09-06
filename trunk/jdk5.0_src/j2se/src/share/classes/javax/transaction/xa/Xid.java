/*
 * @(#)Xid.java	1.5 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package javax.transaction.xa;

/**
 * The Xid interface is a Java mapping of the X/Open transaction identifier 
 * XID structure. This interface specifies three accessor methods to 
 * retrieve a global transaction format ID, global transaction ID, 
 * and branch qualifier. The Xid interface is used by the transaction 
 * manager and the resource managers. This interface is not visible to
 * the application programs.
 */
public interface Xid {
    
    /**
     * Maximum number of bytes returned by getGtrid.
     */
    final static int MAXGTRIDSIZE = 64;

    /**
     * Maximum number of bytes returned by getBqual.
     */
    final static int MAXBQUALSIZE = 64;

    /**
     * Obtain the format identifier part of the XID.
     *
     * @return Format identifier. O means the OSI CCR format.
     */
    int getFormatId();

    /**
     * Obtain the global transaction identifier part of XID as an array 
     * of bytes.
     *
     * @return Global transaction identifier.
     */
    byte[] getGlobalTransactionId();

    /**
     * Obtain the transaction branch identifier part of XID as an array 
     * of bytes.
     *
     * @return Global transaction identifier.
     */
    byte[] getBranchQualifier();
}
