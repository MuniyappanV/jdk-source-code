/*
 * @(#)ByteToCharMS936.java	1.14 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.io;

import sun.nio.cs.ext.MS936;

/**
 * Tables and data to convert MS936 to Unicode
 *
 * @author  ConverterGenerator tool
 * @version >= JDK1.1.6
 */

public class ByteToCharMS936 extends ByteToCharDoubleByte {

    private final static MS936 nioCoder = new MS936();

    public String getCharacterEncoding() {
        return "MS936";
    }

    public ByteToCharMS936() {
        super.index1 = nioCoder.getDecoderIndex1();
        super.index2 = nioCoder.getDecoderIndex2();
        start = 0x40;
        end = 0xFE;
    }
}
