/*
 * @(#)CharToByteCp775.java	1.16	10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.io;

import sun.nio.cs.IBM775;

/**
 * Tables and data to convert Unicode to Cp775
 *
 * @author  ConverterGenerator tool
 * @version >= JDK1.1.6
 */

public class CharToByteCp775 extends CharToByteSingleByte {

    private final static IBM775 nioCoder = new IBM775();

    public String getCharacterEncoding() {
        return "Cp775";
    }

    public CharToByteCp775() {
        super.mask1 = 0xFF00;
        super.mask2 = 0x00FF;
        super.shift = 8;
        super.index1 = nioCoder.getEncoderIndex1();
        super.index2 = nioCoder.getEncoderIndex2();
    }
}
