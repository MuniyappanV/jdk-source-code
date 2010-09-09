/*
 * @(#)ByteToCharCp868.java	1.17	10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */


package sun.io;

import sun.nio.cs.ext.IBM868;

/**
 * A table to convert to Cp868 to Unicode
 *
 * @author  ConverterGenerator tool
 * @version >= JDK1.1.6
 */

public class ByteToCharCp868 extends ByteToCharSingleByte {

    private final static IBM868 nioCoder = new IBM868();

    public String getCharacterEncoding() {
        return "Cp868";
    }

    public ByteToCharCp868() {
        super.byteToCharTable = nioCoder.getDecoderSingleByteMappings();
    }
}
