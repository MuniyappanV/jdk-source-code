/*
 * @(#)CharToByteMacRoman.java	1.14	03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.io;

import sun.nio.cs.ext.MacRoman;

/**
 * Tables and data to convert Unicode to MacRoman
 *
 * @author  ConverterGenerator tool
 * @version >= JDK1.1.6
 */

public class CharToByteMacRoman extends CharToByteSingleByte {

    private final static MacRoman nioCoder = new MacRoman();

    public String getCharacterEncoding() {
        return "MacRoman";
    }

    public CharToByteMacRoman() {
        super.mask1 = 0xFF00;
        super.mask2 = 0x00FF;
        super.shift = 8;
        super.index1 = nioCoder.getEncoderIndex1();
        super.index2 = nioCoder.getEncoderIndex2();
    }
}
