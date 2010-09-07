/*
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (MOTIF).  See the file named COPYRIGHT.MOTIF
 * for the full copyright text.
 * 
 */
/*
 * HISTORY
 */
#ifdef REV_INFO
#ifndef lint
static char rcsid[] = "$XConsortium: XmStringObso.c /main/6 1995/09/19 23:13:52 cde-sun $"
#endif
#endif

#include "XmStringI.h"
#include "XmI.h"

/*
 * as close as we can come to Latin1Create without knowing the charset of
 * Latin1.  This imposes the semantic of \n meaning separator.
 */
XmString 
XmStringLtoRCreate(
        char *text,
        XmStringTag tag )
{
  char *start, *end;
  Boolean done;
  XmString string;

  _XmProcessLock();
  if (!text) {
	_XmProcessUnlock();
	return (NULL);
  }

  start = text;
  done = FALSE;

  /* Set the direction once only at the beginning. */
  string = XmStringDirectionCreate(XmSTRING_DIRECTION_L_TO_R);
    
  while ( ! done)		/* loop thu local copy */
    {				/* looking for \n */
      end = start;

      while ((*end != '\0') && (*end != '\n'))  end++;

      if (*end == '\0')
	done = TRUE;		/* we are at the end */

      /* Don't convert empty string unless it's an initial newline. */
      /* Done so StringHeight has clue to size of empty lines. */
      if ((start != end) || (start == text))
        string = XmStringConcatAndFree (string, 
					_XmStringNCreate(start, tag, 
							 (int)(end - start))); /* Wyoming 64-bit fix */ 
      
      /* Make a separator if this isn't the last segment. */
      if (!done) {
        string = XmStringConcatAndFree(string, XmStringSeparatorCreate());
	start = ++end;		/* start at next char */
      }
    }

  _XmProcessUnlock();
  return (string);
}

XmString 
XmStringCreateLtoR(
        char *text,
        XmStringTag tag )
{
  return (XmStringLtoRCreate (text, tag));
}


/*
 * build an external TCS 'segment', just a high level create
 */
XmString 
XmStringSegmentCreate(
        char *text,
        XmStringTag tag,
#if NeedWidePrototypes
        int direction,
        int separator )
#else
        XmStringDirection direction,
        Boolean separator )
#endif /* NeedWidePrototypes */
{
  XmString result;

  result = XmStringConcatAndFree (XmStringDirectionCreate (direction),
				  XmStringCreate (text, tag));

  if (separator)
    result = XmStringConcatAndFree (result, XmStringSeparatorCreate ());

  return result;
}

/*
 * Convenience routine to create an XmString from a NULL terminated string.
 */
XmString 
XmStringCreateSimple(
        char *text )
{
  return (XmStringCreate(text, XmSTRING_DEFAULT_CHARSET));
}
/*
 * concat two external strings.  Only concat a component at a time
 * so that we always wind up with a meaningful string
 */
XmString 
XmStringNConcat(XmString first,
		XmString second,
		int n )
{
  XmString	tmp, ret_val;
  
  _XmProcessLock();
  tmp = XmStringConcat(first, second);
  
  ret_val = XmStringNCopy(tmp, XmStringLength(first) + n);
  
  XmStringFree(tmp);
  
  _XmProcessUnlock();
  return(ret_val);
}

/*
 * Copy a compound string, such that the equivalent ASN.1 form
 * has <= n bytes.  Only copy a component at a time
 * so that we always wind up with a meaningful string
 */
XmString 
XmStringNCopy(
        XmString str,
        int n )
{
  unsigned char	*tmp;
  unsigned int	len;
  XmString	ret_val;
  
  _XmProcessLock();
  len = XmCvtXmStringToByteStream(str, &tmp);
  
  if (n >= len) /* No need to truncate */
    {
      ret_val = XmStringCopy(str);
    }
  else /* Truncate and convert */
    {
      tmp = _XmStringTruncateASN1(tmp, n);
      ret_val = XmCvtByteStreamToXmString(tmp);
    }
  
  XtFree((char *)tmp);
  
  _XmProcessUnlock();
  return(ret_val);
}

/* Compare ASN.1 form of strings. */
Boolean 
XmStringByteCompare(
        XmString a1,
        XmString b1 )
{
    unsigned char  *a;
    unsigned char  *b;
    unsigned short a_length, b_length;
    Boolean	   ret_val;

    _XmProcessLock();
    if ((a1 == NULL) && (b1 == NULL)) {
	_XmProcessUnlock();
	return (TRUE);
    }
    if ((a1 == NULL) || (b1 == NULL)) {
	_XmProcessUnlock();
	return (FALSE);
    }

    a_length = XmCvtXmStringToByteStream(a1, &a);
    b_length = XmCvtXmStringToByteStream(b1, &b);

    if ((a_length != b_length) || (memcmp(a, b, a_length) != 0))
      ret_val = FALSE;
    else ret_val = TRUE;

    XtFree((char *)a);
    XtFree((char *)b);

    _XmProcessUnlock();
    return(ret_val);
}

