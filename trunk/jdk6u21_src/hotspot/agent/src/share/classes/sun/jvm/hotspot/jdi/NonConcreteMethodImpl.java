/*
 * Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

package sun.jvm.hotspot.jdi;

import com.sun.jdi.*;

import java.util.List;
import java.util.Map;
import java.util.Iterator;
import java.util.ListIterator;
import java.util.HashMap;
import java.util.ArrayList;
import java.util.Collections;

/**
 * Represents non-concrete (that is, native or abstract) methods.
 * Private to MethodImpl.
 */
public class NonConcreteMethodImpl extends MethodImpl {

    private Location location = null;

    NonConcreteMethodImpl(VirtualMachine vm,
                          ReferenceTypeImpl declaringType,
                          sun.jvm.hotspot.oops.Method saMethod) {
        super(vm, declaringType, saMethod);
    }

    public Location location() {
        if (isAbstract()) {
            return null;
        }
        if (location == null) {
            location = new LocationImpl(vm, this, -1);
        }
        return location;
    }

    public List allLineLocations(String stratumID,
                                 String sourceName) {
        return new ArrayList(0);
    }

    public List allLineLocations(SDE.Stratum stratum,
                                 String sourceName) {
        return new ArrayList(0);
    }

    public List locationsOfLine(String stratumID,
                                String sourceName,
                                int lineNumber) {
        return new ArrayList(0);
    }

    public List locationsOfLine(SDE.Stratum stratum,
                                String sourceName,
                                int lineNumber) {
        return new ArrayList(0);
    }

    public Location locationOfCodeIndex(long codeIndex) {
        return null;
    }

    LineInfo codeIndexToLineInfo(SDE.Stratum stratum,
                                 long codeIndex) {

        if (stratum.isJava()) {
            return new BaseLineInfo(-1, declaringType);
        } else {
            return new StratumLineInfo(stratum.id(), -1,
                                       null, null);
        }
    }

    public List variables() throws AbsentInformationException {
        throw new AbsentInformationException();
    }

    public List variablesByName(String name) throws AbsentInformationException {
        throw new AbsentInformationException();
    }

    public List arguments() throws AbsentInformationException {
        throw new AbsentInformationException();
    }

    public byte[] bytecodes() {
        return new byte[0];
    }

    int argSlotCount() throws AbsentInformationException {
        throw new InternalException("should not get here");
    }
}
