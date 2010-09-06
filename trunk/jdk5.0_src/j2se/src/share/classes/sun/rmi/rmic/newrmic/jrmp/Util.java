/*
 * @(#)Util.java	1.2 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.rmi.rmic.newrmic.jrmp;

import com.sun.javadoc.ClassDoc;
import com.sun.javadoc.MethodDoc;
import com.sun.javadoc.Parameter;
import com.sun.javadoc.Type;

/**
 * Provides static utility methods.
 *
 * WARNING: The contents of this source file are not part of any
 * supported API.  Code that depends on them does so at its own risk:
 * they are subject to change or removal without notice.
 *
 * @version 1.2, 03/12/19
 * @author Peter Jones
 **/
final class Util {

    private Util() { throw new AssertionError(); }

    /**
     * Returns the binary name of the class or interface represented
     * by the specified ClassDoc.
     **/
    static String binaryNameOf(ClassDoc cl) {
	String flat = cl.name().replace('.', '$');
	String packageName = cl.containingPackage().name();
	return packageName.equals("") ? flat : packageName + "." + flat;
    }

    /**
     * Returns the method descriptor for the specified method.
     *
     * See section 4.3.3 of The Java Virtual Machine Specification
     * Second Edition for the definition of a "method descriptor".
     **/
    static String methodDescriptorOf(MethodDoc method) {
	String desc = "(";
	Parameter[] parameters = method.parameters();
	for (int i = 0; i < parameters.length; i++) {
	    desc += typeDescriptorOf(parameters[i].type());
	}
	desc += ")" + typeDescriptorOf(method.returnType());
	return desc;
    }

    /**
     * Returns the descriptor for the specified type, as appropriate
     * for either a parameter or return type in a method descriptor.
     **/
    private static String typeDescriptorOf(Type type) {
	String desc;
	ClassDoc classDoc = type.asClassDoc();
	if (classDoc == null) {
	    /*
	     * Handle primitive types.
	     */
	    String name = type.typeName();
	    if (name.equals("boolean")) {
		desc = "Z";
	    } else if (name.equals("byte")) {
		desc = "B";
	    } else if (name.equals("char")) {
		desc = "C";
	    } else if (name.equals("short")) {
		desc = "S";
	    } else if (name.equals("int")) {
		desc = "I";
	    } else if (name.equals("long")) {
		desc = "J";
	    } else if (name.equals("float")) {
		desc = "F";
	    } else if (name.equals("double")) {
		desc = "D";
	    } else if (name.equals("void")) {
		desc = "V";
	    } else {
		throw new AssertionError(
		    "unrecognized primitive type: " + name);
	    }
	} else {
	    /*
	     * Handle non-array reference types.
	     */
	    desc = "L" + binaryNameOf(classDoc).replace('.', '/') + ";";
	}

	/*
	 * Handle array types.
	 */
	int dimensions = type.dimension().length() / 2;
	for (int i = 0; i < dimensions; i++) {
	    desc = "[" + desc;
	}

	return desc;
    }

    /**
     * Returns a reader-friendly string representation of the
     * specified method's signature.  Names of reference types are not
     * package-qualified.
     **/
    static String getFriendlyUnqualifiedSignature(MethodDoc method) {
	String sig = method.name() + "(";
	Parameter[] parameters = method.parameters();
	for (int i = 0; i < parameters.length; i++) {
	    if (i > 0) {
		sig += ", ";
	    }
	    Type paramType = parameters[i].type();
	    sig += paramType.typeName() + paramType.dimension();
	}
	sig += ")";
	return sig;
    }

    /**
     * Returns true if the specified type is void.
     **/
    static boolean isVoid(Type type) {
	return type.asClassDoc() == null && type.typeName().equals("void");
    }
}
