/*
 * @(#)MethodGenClone24.java	1.11 03/12/19
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
 * @(#)MethodGenClone24.java	1.11 03/12/19
 */

package com.sun.tools.corba.se.idl.toJavaPortable;

// NOTES:
// -D62023   <klr> New file to implement CORBA 2.4 RTF
// NOTE: The methods in this class should be exact copies of the 
// correspoind methods in MethodGen24. The purpose of this class is 
// to inject the changes made in MethodGen24 between AttributeGen
// and AttributeGen24. When the AttributeGen24 changes are merged, this
// class should be deleted.

import java.io.PrintWriter;

import java.util.Enumeration;
import java.util.Vector;
import java.util.Hashtable;

import com.sun.tools.corba.se.idl.GenFileStream;
import com.sun.tools.corba.se.idl.InterfaceEntry;
import com.sun.tools.corba.se.idl.MethodEntry;
import com.sun.tools.corba.se.idl.ParameterEntry;
import com.sun.tools.corba.se.idl.SymtabEntry;
import com.sun.tools.corba.se.idl.ValueEntry;
import com.sun.tools.corba.se.idl.ValueBoxEntry;
import com.sun.tools.corba.se.idl.TypedefEntry;
import com.sun.tools.corba.se.idl.InterfaceState;
import com.sun.tools.corba.se.idl.PrimitiveEntry;
import com.sun.tools.corba.se.idl.StructEntry;

/**
 *
 **/
public class MethodGenClone24 extends AttributeGen
{
  /**
   * Public zero-argument constructor.
   **/
  public MethodGenClone24 ()
  {
  } // ctor

  /**
   * <d62023> - write an abstract method definition
   **/
  protected void abstractMethod (Hashtable symbolTable, MethodEntry m, PrintWriter stream)
  {
    this.symbolTable = symbolTable;
    this.m           = m;
    this.stream      = stream;
    if (m.comment () != null)
      m.comment ().generate ("  ", stream);
    stream.print ("  ");
    stream.print ("public abstract ");
    writeMethodSignature ();
    stream.println (";");
    stream.println ();
  } // abstractMethod

  /**
   * <d62023> - delete method templates for valuetypes
   **/
  protected void interfaceMethod (Hashtable symbolTable, MethodEntry m, PrintWriter stream)
  {
    this.symbolTable = symbolTable;
    this.m           = m;
    this.stream      = stream;
    if (m.comment () != null)
      m.comment ().generate ("  ", stream);
    stream.print ("  ");
    writeMethodSignature ();
    stream.println (";");
  } // interfaceMethod
}
