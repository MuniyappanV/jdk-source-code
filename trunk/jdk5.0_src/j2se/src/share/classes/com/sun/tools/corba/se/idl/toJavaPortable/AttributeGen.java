/*
 * @(#)AttributeGen.java	1.14 03/12/19
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
 * @(#)AttributeGen.java	1.14 03/12/19
 */

package com.sun.tools.corba.se.idl.toJavaPortable;

// NOTES:

import java.io.PrintWriter;
import java.util.Enumeration;
import java.util.Hashtable;

import com.sun.tools.corba.se.idl.AttributeEntry;
import com.sun.tools.corba.se.idl.InterfaceEntry;
import com.sun.tools.corba.se.idl.MethodEntry;
import com.sun.tools.corba.se.idl.ParameterEntry;
import com.sun.tools.corba.se.idl.SymtabEntry;

/**
 *
 **/
public class AttributeGen extends MethodGen implements com.sun.tools.corba.se.idl.AttributeGen
{
  /**
   * Public zero-argument constructor.
   **/
  public AttributeGen ()
  {
  } // ctor

  /**
   *
   **/
  private boolean unique (InterfaceEntry entry, String name)
  {
    // Compare the name to the methods of this interface
    Enumeration methods = entry.methods ().elements ();
    while (methods.hasMoreElements ())
    {
      SymtabEntry method = (SymtabEntry)methods.nextElement ();
      if (name.equals (method.name ()))
        return false;
    }

    // Recursively call unique on each derivedFrom interface
    Enumeration derivedFrom = entry.derivedFrom ().elements ();
    while (derivedFrom.hasMoreElements ())
      if (!unique ((InterfaceEntry)derivedFrom.nextElement (), name))
        return false;

    // If the name isn't in any method, nor in any method of the
    // derivedFrom interfaces, then the name is unique.
    return true;
  } // unique

  /**
   * Method generate() is not used in MethodGen.  They are replaced by the
   * more granular interfaceMethod, stub, skeleton, dispatchSkeleton.
   **/
  public void generate (Hashtable symbolTable, AttributeEntry m, PrintWriter stream)
  {
  } // generate

  /**
   *
   **/
  protected void interfaceMethod (Hashtable symbolTable, MethodEntry m, PrintWriter stream)
  {
    AttributeEntry a = (AttributeEntry)m;

    // Generate for the get method
    super.interfaceMethod (symbolTable, a, stream);

    // Generate for the set method if the attribute is not readonly
    if (!a.readOnly ())
    {
      setupForSetMethod ();
      super.interfaceMethod (symbolTable, a, stream);
      clear ();
    }
  } // interfaceMethod

  /**
   *
   **/
  protected void stub (String className, boolean isAbstract, Hashtable symbolTable, MethodEntry m, PrintWriter stream, int index)
  {
    AttributeEntry a = (AttributeEntry)m;

    // Generate for the get method
    super.stub (className, isAbstract, symbolTable, a, stream, index);

    // Generate for the set method if the attribute is not readonly
    if (!a.readOnly ())
    {
      setupForSetMethod ();
      super.stub (className, isAbstract, symbolTable, a, stream, index + 1);
      clear ();
    }
  } // stub

  /**
   *
   **/
  protected void skeleton (Hashtable symbolTable, MethodEntry m, PrintWriter stream, int index)
  {
    AttributeEntry a = (AttributeEntry)m;

    // Generate for the get method
    super.skeleton (symbolTable, a, stream, index);

    // Generate for the set method if the attribute is not readonly
    if (!a.readOnly ())
    {
      setupForSetMethod ();
      super.skeleton (symbolTable, a, stream, index + 1);
      clear ();
    }
  } // skeleton

  /**
   *
   **/
  protected void dispatchSkeleton (Hashtable symbolTable, MethodEntry m, PrintWriter stream, int index)
  {
    AttributeEntry a = (AttributeEntry)m;

    // Generate for the get method
    super.dispatchSkeleton (symbolTable, a, stream, index);

    // Generate for the set method if the attribute is not readonly
    if (!a.readOnly ())
    {
      setupForSetMethod ();
      super.dispatchSkeleton (symbolTable, m, stream, index + 1);
      clear ();
    }
  } // dispatchSkeleton

  private SymtabEntry realType = null;

  /**
   *
   **/
  protected void setupForSetMethod ()
  {
    ParameterEntry parm = Compile.compiler.factory.parameterEntry ();
    parm.type (m.type ());
    parm.name ("new" + Util.capitalize (m.name ()));
    m.parameters ().addElement (parm);
    realType = m.type ();
    m.type (null);
  } // setupForSetMethod

  /**
   *
   **/
  protected void clear ()
  {
    // Set back to normal
    m.parameters ().removeAllElements ();
    m.type (realType);
  } // clear
} // class AttributeGen
