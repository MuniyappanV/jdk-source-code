/*
 * @(#)TypeComponentImpl.java	1.16 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.tools.jdi;

import com.sun.jdi.*;

import java.util.List;

abstract public class TypeComponentImpl extends MirrorImpl
    implements TypeComponent
{
    protected final long ref;
    protected final String name;
    protected final String signature;
    protected final String genericSignature;
    protected final ReferenceTypeImpl declaringType;
    private final int modifiers;

    TypeComponentImpl(VirtualMachine vm, ReferenceTypeImpl declaringType, 
                      long ref,
                      String name, String signature, 
                      String genericSignature, int modifiers) {
        // The generic signature is set when this is created.
        super(vm);
        this.declaringType = declaringType;
        this.ref = ref;
        this.name = name;
        this.signature = signature;
        if (genericSignature != null && genericSignature.length() != 0) {
            this.genericSignature = genericSignature;
        } else {
            this.genericSignature = null;
        }
        this.modifiers = modifiers;
    }

    public String name() {
        return name;     
    }

    public String signature() {
        return signature; 
    }
    public String genericSignature() {
        return genericSignature;
    }

    public int modifiers() {
        return modifiers;
    }
    
    public ReferenceType declaringType() {
        return declaringType;
    }

    public boolean isStatic() {
        return isModifierSet(VMModifiers.STATIC);
    }

    public boolean isFinal() {
        return isModifierSet(VMModifiers.FINAL);
    }

    public boolean isPrivate() {
        return isModifierSet(VMModifiers.PRIVATE);
    }

    public boolean isPackagePrivate() {
        return !isModifierSet(VMModifiers.PRIVATE
                              | VMModifiers.PROTECTED
                              | VMModifiers.PUBLIC);
    }

    public boolean isProtected() {
        return isModifierSet(VMModifiers.PROTECTED);
    }

    public boolean isPublic() {
        return isModifierSet(VMModifiers.PUBLIC);
    }

    public boolean isSynthetic() {
        return isModifierSet(VMModifiers.SYNTHETIC);
    }

    long ref() {
        return ref;
    }

    boolean isModifierSet(int compareBits) {
        return (modifiers & compareBits) != 0;
    }
}
