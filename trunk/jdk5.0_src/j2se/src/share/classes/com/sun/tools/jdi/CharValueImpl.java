/*
 * @(#)CharValueImpl.java	1.16 04/05/05
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.tools.jdi;

import com.sun.jdi.*;

public class CharValueImpl extends PrimitiveValueImpl
                           implements CharValue {
    private char value;

    CharValueImpl(VirtualMachine aVm,char aValue) {
        super(aVm);

        value = aValue;
    }

    public boolean equals(Object obj) {
        if ((obj != null) && (obj instanceof CharValue)) {
            return (value == ((CharValue)obj).value()) &&
                   super.equals(obj);
        } else {
            return false;
        }
    }

    public int hashCode() {
        /*
         * TO DO: Better hash code
         */
        return intValue();
    }

    public int compareTo(CharValue obj) {
        char other = obj.value();
        return value() - other;
    }

    public Type type() {
        return vm.theCharType();
    }

    public char value() {
        return value;
    }

    public boolean booleanValue() {
        return(value == 0)?false:true;
    }

    public byte byteValue() {
        return(byte)value;
    }

    public char charValue() {
        return(char)value;
    }

    public short shortValue() {
        return(short)value;
    }

    public int intValue() {
        return(int)value;
    }

    public long longValue() {
        return(long)value;
    }

    public float floatValue() {
        return(float)value;
    }

    public double doubleValue() {
        return(double)value;
    }

    public String toString() {
        return "" + value;
    }

    byte checkedByteValue() throws InvalidTypeException {
        // Note: since char is unsigned, don't check against MIN_VALUE
        if (value > Byte.MAX_VALUE) {
            throw new InvalidTypeException("Can't convert " + value + " to byte");
        } else {
            return super.checkedByteValue();
        }
    }

    short checkedShortValue() throws InvalidTypeException {
        // Note: since char is unsigned, don't check against MIN_VALUE
        if (value > Short.MAX_VALUE) {
            throw new InvalidTypeException("Can't convert " + value + " to short");
        } else {
            return super.checkedShortValue();
        }
    }

    byte typeValueKey() {
        return JDWP.Tag.CHAR;
    }
}
