/*
 * @(#)XAConnection.java	1.8 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package javax.sql;

import java.sql.*;


/**
 * An object that provides support for distributed 
 * transactions.  An <code>XAConnection</code> object  may be enlisted 
 * in a distributed transaction by means of an <code>XAResource</code> object.
 * A transaction manager, usually part of a middle tier server, manages an
 * <code>XAConnection</code> object through the <code>XAResource</code> object.
 * <P>
 * An application programmer does not use this interface directly; rather,
 * it is used by a transaction manager working in the middle tier server.
 *
 * @since 1.4
 */

public interface XAConnection extends PooledConnection {


  /**
   * Retrieves an <code>XAResource</code> object that 
   * the transaction manager will use 
   * to manage this <code>XAConnection</code> object's participation in a
   * distributed transaction.
   *
   * @return the <code>XAResource</code> object
   * @exception SQLException if a database access error occurs
   */
  javax.transaction.xa.XAResource getXAResource() throws SQLException;

 } 





