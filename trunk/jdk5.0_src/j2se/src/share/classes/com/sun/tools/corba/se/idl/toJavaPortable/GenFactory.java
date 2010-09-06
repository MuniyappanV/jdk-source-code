/*
 * @(#)GenFactory.java	1.13 04/04/07
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
 * @(#)GenFactory.java	1.13 04/04/07
 */

package com.sun.tools.corba.se.idl.toJavaPortable;

// NOTES:

/**
 *
 **/
public class GenFactory implements com.sun.tools.corba.se.idl.GenFactory
{

  public com.sun.tools.corba.se.idl.AttributeGen createAttributeGen ()
  {
    if (Util.corbaLevel (2.4f, 99.0f)) // <d60023>
      return new AttributeGen24 ();
    else
      return new AttributeGen ();
  } // createAttributeGen

  public com.sun.tools.corba.se.idl.ConstGen createConstGen ()
  {
    return new ConstGen ();
  } // createConstGen

  public com.sun.tools.corba.se.idl.NativeGen createNativeGen ()
  {
    return new NativeGen ();
  } // createNativeGen

  public com.sun.tools.corba.se.idl.EnumGen createEnumGen ()
  {
    return new EnumGen ();
  } // createEnumGen

  public com.sun.tools.corba.se.idl.ExceptionGen createExceptionGen ()
  {
    return new ExceptionGen ();
  } // createExceptionGen

  public com.sun.tools.corba.se.idl.ForwardGen createForwardGen ()
  {
    return null;
  } // createForwardGen

  public com.sun.tools.corba.se.idl.ForwardValueGen createForwardValueGen ()
  {
    return null;
  } // createForwardValueGen

  public com.sun.tools.corba.se.idl.IncludeGen createIncludeGen ()
  {
    return null;
  } // createIncludeGen

  public com.sun.tools.corba.se.idl.InterfaceGen createInterfaceGen ()
  {
    return new InterfaceGen ();
  } // createInterfaceGen

  public com.sun.tools.corba.se.idl.ValueGen createValueGen ()
  {
    if (Util.corbaLevel (2.4f, 99.0f)) // <d60023>
      return new ValueGen24 ();
    else
      return new ValueGen ();
  } // createValueGen

  public com.sun.tools.corba.se.idl.ValueBoxGen createValueBoxGen ()
  {
    if (Util.corbaLevel (2.4f, 99.0f)) // <d60023>
      return new ValueBoxGen24 ();
    else
      return new ValueBoxGen ();
  } // createValueBoxGen

  public com.sun.tools.corba.se.idl.MethodGen createMethodGen ()
  {
    if (Util.corbaLevel (2.4f, 99.0f)) // <d60023>
      return new MethodGen24 ();
    else
      return new MethodGen ();
  } // createMethodGen

  public com.sun.tools.corba.se.idl.ModuleGen createModuleGen ()
  {
    return new ModuleGen ();
  } // createModuleGen

  public com.sun.tools.corba.se.idl.ParameterGen createParameterGen ()
  {
    return null;
  } // createParameterGen

  public com.sun.tools.corba.se.idl.PragmaGen createPragmaGen ()
  {
    return null;
  } // createPragmaGen

  public com.sun.tools.corba.se.idl.PrimitiveGen createPrimitiveGen ()
  {
    return new PrimitiveGen ();
  } // createPrimitiveGen

  public com.sun.tools.corba.se.idl.SequenceGen createSequenceGen ()
  {
    return new SequenceGen ();
  } // createSequenceGen

  public com.sun.tools.corba.se.idl.StringGen createStringGen ()
  {
    return new StringGen ();
  } // createSequenceGen

  public com.sun.tools.corba.se.idl.StructGen createStructGen ()
  {
    return new StructGen ();
  } // createStructGen

  public com.sun.tools.corba.se.idl.TypedefGen createTypedefGen ()
  {
    return new TypedefGen ();
  } // createTypedefGen

  public com.sun.tools.corba.se.idl.UnionGen createUnionGen ()
  {
    return new UnionGen ();
  } // createUnionGen
} // class GenFactory
