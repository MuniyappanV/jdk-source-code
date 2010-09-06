/*
 * @(#)BooleanNot.java	1.12 03/12/19
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
 * @(#)BooleanNot.java	1.12 03/12/19
 */

package com.sun.tools.corba.se.idl.constExpr;

// NOTES:

import com.sun.tools.corba.se.idl.Util;
import java.math.BigInteger;

public class BooleanNot extends UnaryExpr
{
  protected BooleanNot (Expression operand)
  {
    super ("!", operand);
  } // ctor

  public Object evaluate () throws EvaluationException
  {
    try
    {
      Object tmp = operand ().evaluate ();
      Boolean op;
      //daz      if (tmp instanceof Number)
      //           op = new Boolean (((Number)tmp).longValue () != 0);
      //         else
      //           op = (Boolean)tmp;
      if (tmp instanceof Number)
      {
        if (tmp instanceof BigInteger)
          op = new Boolean (((BigInteger)tmp).compareTo (zero) != 0);
        else
          op = new Boolean (((Number)tmp).longValue () != 0);
      }
      else
        op = (Boolean)tmp;

      value (new Boolean (!op.booleanValue ()));
    }
    catch (ClassCastException e)
    {
      String[] parameters = {Util.getMessage ("EvaluationException.booleanNot"), operand ().value ().getClass ().getName ()};
      throw new EvaluationException (Util.getMessage ("EvaluationException.2", parameters));
    }
    return value ();
  } // evaluate
} // class BooleanNot
