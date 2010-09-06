/*
 * @(#)MessageUtils.java	1.17 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.misc;

/**
 * MessageUtils: miscellaneous utilities for handling error and status
 * properties and messages.
 *
 * @version 1.17, 12/19/03
 * @author Herb Jellinek
 */

public class MessageUtils {
    // can instantiate it for to allow less verbose use - via instance
    // instead of classname
    
    public MessageUtils() { }

    public static String subst(String patt, String arg) {
	String args[] = { arg };
	return subst(patt, args);
    }

    public static String subst(String patt, String arg1, String arg2) {
	String args[] = { arg1, arg2 };
	return subst(patt, args);
    }

    public static String subst(String patt, String arg1, String arg2,
			       String arg3) {
	String args[] = { arg1, arg2, arg3 };
	return subst(patt, args);
    }

    public static String subst(String patt, String args[]) {
	StringBuffer result = new StringBuffer();
	int len = patt.length();
	for (int i = 0; i >= 0 && i < len; i++) {
	    char ch = patt.charAt(i);
	    if (ch == '%') {
		if (i != len) {
		    int index = Character.digit(patt.charAt(i + 1), 10);
		    if (index == -1) {
			result.append(patt.charAt(i + 1));
			i++;
		    } else if (index < args.length) {
			result.append(args[index]);
			i++;
		    }
		}
	    } else {
		result.append(ch);
	    }
	}
	return result.toString();
    }

    public static String substProp(String propName, String arg) {
	return subst(System.getProperty(propName), arg);
    }

    public static String substProp(String propName, String arg1, String arg2) {
	return subst(System.getProperty(propName), arg1, arg2);
    }

    public static String substProp(String propName, String arg1, String arg2,
				   String arg3) {
	return subst(System.getProperty(propName), arg1, arg2, arg3);
    }

    /**
     *  Print a message directly to stderr, bypassing all the
     *  character conversion methods. 
     *  @param msg   message to print
     */
    public static native void toStderr(String msg);

    /**
     *  Print a message directly to stdout, bypassing all the
     *  character conversion methods. 
     *  @param msg   message to print
     */
    public static native void toStdout(String msg);


    // Short forms of the above

    public static void err(String s) {
	toStderr(s + "\n");
    }

    public static void out(String s) {
	toStdout(s + "\n");
    }

    // Print a stack trace to stderr
    //
    public static void where() {
	Throwable t = new Throwable();
	StackTraceElement[] es = t.getStackTrace();
	for (int i = 1; i < es.length; i++)
	    toStderr("\t" + es[i].toString() + "\n");
    }

}
