/*
 * @(#)SequenceEntry.java	1.12 04/04/07
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
/*
 * COMPONENT_NAME: idl.parser
 *
 * ORIGINS: 27
 *
 * Licensed Materials - Property of IBM
 * 5639-D57 (C) COPYRIGHT International Business Machines Corp. 1997, 1999
 * RMI-IIOP v1.0
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * @(#)SequenceEntry.java	1.12 04/04/07
 */

package com.sun.tools.corba.se.idl;

// NOTES:

import java.io.PrintWriter;
import java.util.Hashtable;
import java.util.Vector;

import com.sun.tools.corba.se.idl.constExpr.Expression;

/**
 * This is the symbol table entry for sequences.
 **/
public class SequenceEntry extends SymtabEntry
{
  protected SequenceEntry ()
  {
    super ();
    repositoryID (Util.emptyID);
  } // ctor

  protected SequenceEntry (SequenceEntry that)
  {
    super (that);
    _maxSize = that._maxSize;
  } // ctor

  protected SequenceEntry (SymtabEntry that, IDLID clone)
  {
    super (that, clone);
    if (!(that instanceof SequenceEntry))
      // If that is a SequenceEntry, then it is a container of this sequence, but it is not a module of this sequence.  It's name doesn't belong in the module name.
      if (module ().equals (""))
        module (name ());
      else if (!name ().equals (""))
        module (module () + "/" + name ());
    repositoryID (Util.emptyID);
  } // ctor

  public Object clone ()
  {
    return new SequenceEntry (this);
  } // clone

  public boolean isReferencable()
  {
    // A sequence is referencable if its component
    // type is.
    return type().isReferencable() ;
  }

  public void isReferencable( boolean value ) 
  {
    // NO-OP: this cannot be set for a sequence.
  }

  /** Invoke the sequence generator.
      @param symbolTable the symbol table is a hash table whose key is
       a fully qualified type name and whose value is a SymtabEntry or
       a subclass of SymtabEntry.
      @param stream the stream to which the generator should sent its output.
      @see SymtabEntry */
  public void generate (Hashtable symbolTable, PrintWriter stream)
  {
    sequenceGen.generate (symbolTable, this, stream);
  } // generate

  /** Access the sequence generator.
      @returns an object which implements the SequenceGen interface.
      @see SequenceGen */
  public Generator generator ()
  {
    return sequenceGen;
  } // generator

  /** the constant expression defining the maximum size of the sequence.
      If it is null, then the sequence is unbounded. */
  public void maxSize (Expression expr)
  {
    _maxSize = expr;
  } // maxSize

  /** the constant expression defining the maximum size of the sequence.
      If it is null, then the sequence is unbounded. */
  public Expression maxSize ()
  {
    return _maxSize;
  } // maxSize

  /** Only sequences can be contained within sequences. */
  public void addContained (SymtabEntry entry)
  {
    _contained.addElement (entry);
  } // addContained

  /** Only sequences can be contained within sequences. */
  public Vector contained ()
  {
    return _contained;
  } // contained

  static SequenceGen sequenceGen;

  private Expression _maxSize   = null;
  private Vector     _contained = new Vector ();
} // class SequenceEntry
