/*
 * @(#)ByteToCharCp964.java	1.14 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package sun.io;

import sun.nio.cs.ext.IBM964;

/**
* @author Malcolm Ayres
*/
public class ByteToCharCp964 extends ByteToCharConverter
{
    private final static IBM964 nioCoder = new IBM964();

    private final int G0 = 0;
    private final int G1 = 1;
    private final int G2 = 2;
    private final int G3 = 3;
    private final int G4 = 4;
    private final int SS2 =  0x8E;
    private final int SS3 =  0x8F;

    private int firstByte, state;

    private String byteToCharTable;
    private String mappingTableG1;
    private String mappingTableG2;
    private String mappingTableG2a2;
    private String mappingTableG2ac;
    private String mappingTableG2ad;


    public ByteToCharCp964() {
       super();
       state = G0;
       byteToCharTable = nioCoder.getDecoderSingleByteMappings(); 
       mappingTableG1 = nioCoder.getDecoderMappingTableG1();
       mappingTableG2a2 = nioCoder.getDecoderMappingTableG2a2();
       mappingTableG2ac = nioCoder.getDecoderMappingTableG2ac();
       mappingTableG2ad = nioCoder.getDecoderMappingTableG2ad();
    }

    /**
      * Return the character set id
      */
    public String getCharacterEncoding()
    {
       return "Cp964";
    }

    /**
      * flush out any residual data and reset the buffer state
      */
    public int flush(char[] output, int outStart, int outEnd)
       throws MalformedInputException
    {
       if (state != G0) {
          reset();
          badInputLength = 0;
          throw new MalformedInputException();
       }

       reset();
       return 0;
    }

    /**
     *  Resets the converter.
     */
    public void reset() {
       state = G0;
       charOff = byteOff = 0;
    }

    /**
     * Character conversion
     */
    public int convert(byte[] input, int inOff, int inEnd,
                       char[] output, int outOff, int outEnd)
        throws UnknownCharacterException, MalformedInputException,
               ConversionBufferFullException
    {

       int       byte1;
       char      outputChar = '\uFFFD';

       byteOff = inOff;
       charOff = outOff;

       while (byteOff < inEnd) {

          byte1 = input[byteOff];
          if (byte1 < 0)
            byte1 += 256;

          switch (state) {
             case G0:
                if (byte1 == SS2)
                   state = G2;
                else if (byte1 == SS3) {
                   badInputLength = 1;
                   throw new MalformedInputException();
                }
                else if ( byte1 <= 0x9f )               // valid single byte
                   outputChar = byteToCharTable.charAt(byte1);
                else if (byte1 < 0xa1 || byte1 > 0xfe) {
                   badInputLength = 1;
                   throw new MalformedInputException();
                } else {                                // valid 1st byte for G1
                   firstByte = byte1;
                   state = G1;
                }
                break;

             case G1:
                state = G0;
                if ( byte1 < 0xa1 || byte1 > 0xfe) {   // valid second byte for G1
                   badInputLength = 1;
                   throw new MalformedInputException();
                }
                outputChar = mappingTableG1.charAt(((firstByte - 0xa1) * 94)  + byte1 - 0xa1);
                break;

             case G2:
                // set the correct mapping table for supported G2 sets
                if ( byte1 == 0xa2)
                  mappingTableG2 = mappingTableG2a2;
                else
                if ( byte1 == 0xac)
                  mappingTableG2 = mappingTableG2ac;
                else
                if ( byte1 == 0xad)
                  mappingTableG2 = mappingTableG2ad;
                else {
                   state = G0;
                   badInputLength = 1;
                   throw new MalformedInputException();
                }
                state = G3;
                break;

             case G3:
                if ( byte1 < 0xa1 || byte1 > 0xfe) {  // valid 1st byte for G2 set
                   state = G0;
                   badInputLength = 1;
                   throw new MalformedInputException();
                }
                firstByte = byte1;
                state = G4;
                break;

             case G4:
                state = G0;
                if ( byte1 < 0xa1 || byte1 > 0xfe) { // valid 2nd byte for G2 set
                   badInputLength = 1;
                   throw new MalformedInputException();
                }
                outputChar = mappingTableG2.charAt(((firstByte - 0xa1) * 94)  + byte1 - 0xa1);
                break;

          }

          if (state == G0) {
             if (outputChar == '\uFFFD') {
                if (subMode)
                   outputChar = subChars[0];
                else {
                   badInputLength = 1;
                   throw new UnknownCharacterException();
                }
             }

             if (charOff >= outEnd)
                throw new ConversionBufferFullException();

             output[charOff++] = outputChar;
          }

          byteOff++;

       }

       return charOff - outOff;

    }
}
