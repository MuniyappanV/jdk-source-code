/*
 * @(#)ShiftRight.java	1.12 03/12/19
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
 * @(#)ShiftRight.java	1.12 03/12/19
 */

package com.sun.tools.corba.se.idl.constExpr;

// NOTES:

import com.sun.tools.corba.se.idl.Util;
import java.math.BigInteger;

public class ShiftRight extends BinaryExpr
{
  protected ShiftRight (Expression leftOperand, Expression rightOperand)
  {
    super (">>", leftOperand, rightOperand);
  } // ctor

  public Object evaluate () throws EvaluationException
  {
    try
    {
      Number l = (Number)left ().evaluate ();
      Number r = (Number)right ().evaluate ();

      if (l instanceof Float || l instanceof Double || r instanceof Float || r instanceof Double)
      {
        String[] parameters = {Util.getMessage ("EvaluationException.right"), left ().value ().getClass ().getName (), right ().value ().getClass ().getName ()};
        throw new EvaluationException (Util.getMessage ("EvaluationException.1", parameters));
      }
      else
      {
        // Shift right (>>)
        //daz        value (new Long (l.longValue () >> r.longValue ()));
        BigInteger bL = (BigInteger)coerceToTarget ((BigInteger)l);
        BigInteger bR = (BigInteger)r;

        // Change signed to unsigned (Clear sign bit--can be done when setting bL!)
        if (bL.signum () == -1)
          if (type ().equals ("short"))
            bL = bL.add (twoPow16);
          else if (type ().equals ("long"))
            bL = bL.add (twoPow32);
          else if (type ().equals ("long long"))
            bL = bL.add (twoPow64);

        value (bL.shiftRight (bR.intValue ()));
      }
    }
    catch (ClassCastException e)
    {
      String[] parameters = {Util.getMessage ("EvaluationException.right"), left ().value ().getClass ().getName (), right ().value ().getClass ().getName ()};
      throw new EvaluationException (Util.getMessage ("EvaluationException.1", parameters));
    }
    return value ();
  } // evaluate
} // class ShiftRight
