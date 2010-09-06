/*
 * @(#)JavaGenerator.java	1.12 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
/*
 * COMPONENT_NAME: idl.toJava
 *
 * ORIGINS: 27
 *
 * Licensed Materials - Property of IBM
 * 5639-D57 (C) COPYRIGHT International Business Machines Corp. 1997, 1999
 * RMI-IIOP v1.0
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * @(#)JavaGenerator.java	1.12 03/12/19
 */

package com.sun.tools.corba.se.idl.toJavaPortable;

// NOTES:

import java.io.PrintWriter;

import com.sun.tools.corba.se.idl.SymtabEntry;

/**
 *
 **/
public interface JavaGenerator
{
  // The helper methods print the specific helper method.
  // The helper read/write methods call the read/write methods.

  int helperType (int index, String indent, TCOffsets tcoffsets, String name, SymtabEntry entry, PrintWriter stream);

  void helperRead (String entryName, SymtabEntry entry, PrintWriter stream);

  void helperWrite (SymtabEntry entry, PrintWriter stream);

  // The read/write methods print the streaming of the type.
  // This printed code is found in the helper method but it is only
  // that code that is concerned with streaming itself.

  int read (int index, String indent, String name, SymtabEntry entry, PrintWriter stream);

  int write (int index, String indent, String name, SymtabEntry entry, PrintWriter stream);

  int type (int index, String indent, TCOffsets tcoffsets, String name, SymtabEntry entry, PrintWriter stream);

} // interface JavaGenerator
