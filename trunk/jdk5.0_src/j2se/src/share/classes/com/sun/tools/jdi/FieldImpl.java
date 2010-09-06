/*
 * @(#)FieldImpl.java	1.24 04/05/05
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.tools.jdi;

import com.sun.jdi.*;

import java.util.List;

public class FieldImpl extends TypeComponentImpl 
                       implements Field, ValueContainer {

    FieldImpl(VirtualMachine vm, ReferenceTypeImpl declaringType, 
              long ref,
              String name, String signature, 
              String genericSignature, int modifiers) {
        super(vm, declaringType, ref, name, signature, 
              genericSignature, modifiers);
    }

    public boolean equals(Object obj) {
        if ((obj != null) && (obj instanceof FieldImpl)) {
            FieldImpl other = (FieldImpl)obj;
            return (declaringType().equals(other.declaringType())) &&
                   (ref() == other.ref()) &&
                   super.equals(obj);
        } else {
            return false;
        }
    }

    public int hashCode() {
        return (int)ref();
    }

    public int compareTo(Field field) {
        ReferenceTypeImpl declaringType = (ReferenceTypeImpl)declaringType();
        int rc = declaringType.compareTo(field.declaringType());
        if (rc == 0) {
            rc = declaringType.indexOf(this) - 
                 declaringType.indexOf(field);
        }
        return rc;
    }
    
    public Type type() throws ClassNotLoadedException {
        return findType(signature());
    }

    public Type findType(String signature) throws ClassNotLoadedException {
        ReferenceTypeImpl enclosing = (ReferenceTypeImpl)declaringType(); 
        return enclosing.findType(signature);
    }

    /**
     * @return a text representation of the declared type
     * of this field.
     */
    public String typeName() {
        JNITypeParser parser = new JNITypeParser(signature());
        return parser.typeName();
    }

    public boolean isTransient() {
        return isModifierSet(VMModifiers.TRANSIENT);
    }

    public boolean isVolatile() {
        return isModifierSet(VMModifiers.VOLATILE);
    }

    public boolean isEnumConstant() {
        return isModifierSet(VMModifiers.ENUM_CONSTANT);
    }

    public String toString() {
        StringBuffer buf = new StringBuffer();

        buf.append(declaringType().name());
        buf.append('.');
        buf.append(name());

        return buf.toString();
    }
}
