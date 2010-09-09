/*
 * @(#)CharToByteCp864.java	1.15	10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.io;

import sun.nio.cs.ext.IBM864;

/**
 * Tables and data to convert Unicode to Cp864
 *
 * @author  ConverterGenerator tool
 * @version >= JDK1.1.6
 */

public class CharToByteCp864 extends CharToByteSingleByte {

    private final static IBM864 nioCoder = new IBM864();

    public String getCharacterEncoding() {
        return "Cp864";
    }

    public CharToByteCp864() {
        super.mask1 = 0xFF00;
        super.mask2 = 0x00FF;
        super.shift = 8;
        super.index1 = nioCoder.getEncoderIndex1();
        super.index2 = nioCoder.getEncoderIndex2();
    }
}
