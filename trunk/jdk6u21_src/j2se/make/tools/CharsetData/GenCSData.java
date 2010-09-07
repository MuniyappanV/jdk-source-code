/*
 * @(#)GenCSData.java	1.3 10/03/23
 *
 * Copyright (c) 2008, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

import java.io.*;
import java.util.regex.*;
import static sun.nio.cs.CharsetMapping.*;

public class GenCSData {
    public static void main(String argv[]) throws IOException {
        if (argv.length < 2) {
	    System.out.println("Usage: java GenCSData fMap fDat");
	    System.exit(1);
        }
        genDataJIS0213(new FileInputStream(argv[0]),
		       new FileOutputStream(argv[1]));
    }

    // regex pattern to parse the "jis0213.map" file
    static Pattern sjis0213 = Pattern.compile("0x(\\p{XDigit}++)\\s++U\\+(\\p{XDigit}++)(?:\\+(\\p{XDigit}++))?\\s++#.*");
    private static void genDataJIS0213(InputStream in, OutputStream out)
    {
        int[] sb = new int[0x100];                         // singlebyte
	int[] db = new int[0x10000];                       // doublebyte
	int[] indexC2B = new int[256];
	Entry[] supp = new Entry[0x10000];
	Entry[] comp = new Entry[0x100];
	int suppTotal = 0;
	int compTotal = 0;

	int b1Min1 = 0x81;
	int b1Max1 = 0x9f;
	int b1Min2 = 0xe0;
	int b1Max2 = 0xfc;
	int b2Min = 0x40;
	int b2Max = 0xfe;

	//init
	for (int i = 0; i < 0x80; i++) sb[i] = i;
	for (int i = 0x80; i < 0x100; i++) sb[i] = UNMAPPABLE_DECODING;
	for (int i = 0; i < 0x10000; i++) db[i] = UNMAPPABLE_DECODING;
        try {
	    Parser p = new Parser(in, sjis0213);
	    Entry  e = null;
            while ((e = p.next()) != null) {
                if (e.cp2 != 0) {
		    comp[compTotal++] = e;
                } else {
		    if (e.cp <= 0xffff) {
		        if (e.bs <= 0xff)
			    sb[e.bs] = e.cp;
			else
			    db[e.bs] = e.cp;
			indexC2B[e.cp>>8] = 1;
		    } else {
			supp[suppTotal++] = e;
		    }
		}
            }
	    ByteArrayOutputStream baos = new ByteArrayOutputStream();
	    // c2b Index Table, always the first one
	    writeINDEXC2B(baos, indexC2B);
	    writeSINGLEBYTE(baos, sb);
	    writeDOUBLEBYTE1(baos, db, b1Min1, b1Max1, b2Min, b2Max);
	    writeDOUBLEBYTE2(baos, db, b1Min2, b1Max2, b2Min, b2Max);
	    writeSUPPLEMENT(baos, supp, suppTotal);
	    writeCOMPOSITE(baos, comp, compTotal);
	    writeSIZE(out, baos.size());
	    baos.writeTo(out);
	    out.close();
	} catch (Exception x) {
	    x.printStackTrace();
	}
    }
}
