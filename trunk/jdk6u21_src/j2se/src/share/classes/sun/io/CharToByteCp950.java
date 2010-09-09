/*
 * @(#)CharToByteCp950.java	1.14 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package sun.io;

import sun.nio.cs.ext.IBM950;

/**
* Tables and data to convert Unicode to Cp950
*
* @author Malcolm Ayres, assisted by UniMap program
*/
public class CharToByteCp950
        extends CharToByteDBCS_ASCII

{
        private static IBM950 nioCoder = new IBM950();

        // Return the character set id
        public String getCharacterEncoding()
        {
                return "Cp950";
        }

        public CharToByteCp950()
        {
                super();
                super.mask1 = 0xFFC0;
                super.mask2 = 0x003F;
                super.shift = 6;
                super.index1 = nioCoder.getEncoderIndex1();
                super.index2 = nioCoder.getEncoderIndex2();
                super.index2a = nioCoder.getEncoderIndex2a();
        }
}
