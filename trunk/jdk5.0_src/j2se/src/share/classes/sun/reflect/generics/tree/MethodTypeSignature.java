/*
 * @(#)MethodTypeSignature.java	1.2 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.reflect.generics.tree;

import sun.reflect.generics.visitor.Visitor;

public class MethodTypeSignature implements Signature {
    private FormalTypeParameter[] formalTypeParams;
    private TypeSignature[] parameterTypes;
    private ReturnType returnType;
    private FieldTypeSignature[] exceptionTypes;

    private MethodTypeSignature(FormalTypeParameter[] ftps, 
				TypeSignature[] pts,
				ReturnType rt, 
				FieldTypeSignature[] ets) {
	formalTypeParams = ftps;
	parameterTypes = pts;
	returnType = rt;
	exceptionTypes = ets;
    }

    public static MethodTypeSignature make(FormalTypeParameter[] ftps, 
					   TypeSignature[] pts,
					   ReturnType rt, 
					   FieldTypeSignature[] ets) {
	return new MethodTypeSignature(ftps, pts, rt, ets);
    }

    public FormalTypeParameter[] getFormalTypeParameters(){
	return formalTypeParams;
    }
    public TypeSignature[] getParameterTypes(){return parameterTypes;}
    public ReturnType getReturnType(){return returnType;}
    public FieldTypeSignature[] getExceptionTypes(){return exceptionTypes;}

    public void accept(Visitor v){v.visitMethodTypeSignature(this);}
}
