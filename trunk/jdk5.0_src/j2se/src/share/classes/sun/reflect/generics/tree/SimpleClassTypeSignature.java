/*
 * @(#)SimpleClassTypeSignature.java	1.3 04/04/12
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.reflect.generics.tree;

import sun.reflect.generics.visitor.TypeTreeVisitor;

public class SimpleClassTypeSignature implements FieldTypeSignature {
    private boolean dollar;
    private String name;
    private TypeArgument[] typeArgs;

    private SimpleClassTypeSignature(String n, boolean dollar, TypeArgument[] tas) {
        name = n;
	this.dollar = dollar;
	typeArgs = tas;
    }

    public static SimpleClassTypeSignature make(String n, 
						boolean dollar,
						TypeArgument[] tas){
	return new SimpleClassTypeSignature(n, dollar, tas);
    }

    /*
     * Should a '$' be used instead of '.' to separate this component
     * of the name from the previous one when composing a string to
     * pass to Class.forName; in other words, is this a transition to
     * a nested class.
     */
    public boolean getDollar(){return dollar;}
    public String getName(){return name;}
    public TypeArgument[] getTypeArguments(){return typeArgs;}

    public void accept(TypeTreeVisitor<?> v){
	v.visitSimpleClassTypeSignature(this);
    }
}
