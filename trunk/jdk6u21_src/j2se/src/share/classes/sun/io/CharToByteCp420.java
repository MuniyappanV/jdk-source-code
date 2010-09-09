/*
 * @(#)CharToByteCp420.java	1.21 10/04/21
 *
 * Copyright (c) 2006,2010 Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.io;

import sun.nio.cs.ext.IBM420;

/**
 * Tables and data to convert Unicode to Cp420
 *
 * @author  ConverterGenerator tool
 * @version >= JDK1.1.6
 */

public class CharToByteCp420 extends CharToByteSingleByte2 {

    private final static IBM420 nioCoder = new IBM420();

    public String getCharacterEncoding() {
        return "Cp420";
    }

    public CharToByteCp420() {
        super.mask1 = 0xFF00;
        super.mask2 = 0x00FF;
        super.shift = 8;
        super.index1 = nioCoder.getEncoderIndex1();
        super.index2 = nioCoder.getEncoderIndex2();
    }
}
