/*
 * Copyright (c) 2002, 2003, Oracle and/or its affiliates. All rights reserved.
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

class BaseLineInfo implements LineInfo {
    private final int lineNumber;
    private final ReferenceTypeImpl declaringType;

    BaseLineInfo(int lineNumber,
                 ReferenceTypeImpl declaringType) {
        this.lineNumber = lineNumber;
        this.declaringType = declaringType;
    }

    public String liStratum() {
        return SDE.BASE_STRATUM_NAME;
    }

    public int liLineNumber() {
        return lineNumber;
    }

    public String liSourceName()
                            throws AbsentInformationException {
        return declaringType.baseSourceName();
    }

    public String liSourcePath()
                            throws AbsentInformationException {
        return declaringType.baseSourcePath();
    }
}
