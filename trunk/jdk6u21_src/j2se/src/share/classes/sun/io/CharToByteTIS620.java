/*
 * @(#)CharToByteTIS620.java	1.14 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.io;

import sun.nio.cs.ext.TIS_620;

/**
 * Tables and data to convert Unicode to TIS620
 *
 * @author  ConverterGenerator tool
 * @version >= JDK1.1.6
 */

public class CharToByteTIS620 extends CharToByteSingleByte {

    private final static TIS_620 nioCoder = new TIS_620();

    public String getCharacterEncoding() {
        return "TIS620";
    }

    public CharToByteTIS620() {
        super.mask1 = 0xFF00;
        super.mask2 = 0x00FF;
        super.shift = 8;
        super.index1 = nioCoder.getEncoderIndex1();
        super.index2 = nioCoder.getEncoderIndex2();
    }
}
