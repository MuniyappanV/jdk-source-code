/*
 * @(#)CharToByteCp1254.java	1.17 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.io;

import sun.nio.cs.MS1254;

/**
 * Tables and data to convert Unicode to Cp1254
 *
 * @author  ConverterGenerator tool
 * @version >= JDK1.1.6
 */

public class CharToByteCp1254 extends CharToByteSingleByte {

    private final static MS1254 nioCoder = new MS1254();

    public String getCharacterEncoding() {
        return "Cp1254";
    }

    public CharToByteCp1254() {
        super.mask1 = 0xFF00;
        super.mask2 = 0x00FF;
        super.shift = 8;
        super.index1 = nioCoder.getEncoderIndex1();
        super.index2 = nioCoder.getEncoderIndex2();
    }
}
