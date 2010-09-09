/*
 * @(#)CharToByteISO8859_7.java	1.21 10/03/23
 *
 * Copyright (c) 2006 Sun Microsystems, Inc.,
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 */

package sun.io;

import sun.nio.cs.ISO_8859_7;


/**
 * Tables and data to convert Unicode to ISO8859_7
 *
 * @author  ConverterGenerator tool
 * @version >= JDK1.1.6
 */

public class CharToByteISO8859_7 extends CharToByteSingleByte {

    public String getCharacterEncoding() {
        return "ISO8859_7";
    }

    private final static ISO_8859_7 nioCoder = new ISO_8859_7();

    public CharToByteISO8859_7() {
        super.mask1 = 0xFF00;
        super.mask2 = 0x00FF;
        super.shift = 8;
        super.index1 = nioCoder.getEncoderIndex1();
        super.index2 = nioCoder.getEncoderIndex2();
    }
}
