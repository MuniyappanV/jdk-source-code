/*
 * @(#)LocaleElements_iw.java	1.17 03/12/19
 */

/*
 * Portions Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

/*
 * (C) Copyright Taligent, Inc. 1996, 1997 - All Rights Reserved
 * (C) Copyright IBM Corp. 1996 - 1998 - All Rights Reserved
 *
 * The original version of this source code and documentation
 * is copyrighted and owned by Taligent, Inc., a wholly-owned
 * subsidiary of IBM. These materials are provided under terms
 * of a License Agreement between Taligent and Sun. This technology
 * is protected by multiple US and International patents.
 *
 * This notice and attribution to Taligent may not be removed.
 * Taligent is a registered trademark of Taligent, Inc.
 *
 */

package sun.text.resources;

import java.util.ListResourceBundle;

public class LocaleElements_iw extends ListResourceBundle {
    /**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return new Object[][] {
            { "Languages", // language names
                new String[][] {
                    { "iw", "\u05e2\u05d1\u05e8\u05d9\u05ea" }
                }
            },
            { "Countries", // country names
                new String[][] {
                    { "IL", "\u05d9\u05e9\u05e8\u05d0\u05dc" }
                }
            },
            { "MonthNames",
                new String[] {
                    "\u05d9\u05e0\u05d5\u05d0\u05e8", // january
                    "\u05e4\u05d1\u05e8\u05d5\u05d0\u05e8", // february
                    "\u05de\u05e8\u05e5", // march
                    "\u05d0\u05e4\u05e8\u05d9\u05dc", // april
                    "\u05de\u05d0\u05d9", // may
                    "\u05d9\u05d5\u05e0\u05d9", // june
                    "\u05d9\u05d5\u05dc\u05d9", // july
                    "\u05d0\u05d5\u05d2\u05d5\u05e1\u05d8", // august
                    "\u05e1\u05e4\u05d8\u05de\u05d1\u05e8", // september
                    "\u05d0\u05d5\u05e7\u05d8\u05d5\u05d1\u05e8", // october
                    "\u05e0\u05d5\u05d1\u05de\u05d1\u05e8", // november
                    "\u05d3\u05e6\u05de\u05d1\u05e8", // december
                    "" // month 13 if applicable
                }
            },
            { "MonthAbbreviations",
                new String[] {
                    "\u05d9\u05e0\u05d5", // abb january
                    "\u05e4\u05d1\u05e8", // abb february
                    "\u05de\u05e8\u05e5", // abb march
                    "\u05d0\u05e4\u05e8", // abb april
                    "\u05de\u05d0\u05d9", // abb may
                    "\u05d9\u05d5\u05e0", // abb june
                    "\u05d9\u05d5\u05dc", // abb july
                    "\u05d0\u05d5\u05d2", // abb august
                    "\u05e1\u05e4\u05d8", // abb september
                    "\u05d0\u05d5\u05e7", // abb october
                    "\u05e0\u05d5\u05d1", // abb november
                    "\u05d3\u05e6\u05de", // abb december
                    "" // abb month 13 if applicable
                }
            },
            { "DayNames",
                new String[] {
                    "\u05d9\u05d5\u05dd \u05e8\u05d0\u05e9\u05d5\u05df", // Sunday
                    "\u05d9\u05d5\u05dd \u05e9\u05e0\u05d9", // Monday
                    "\u05d9\u05d5\u05dd \u05e9\u05dc\u05d9\u05e9\u05d9", // Tuesday
                    "\u05d9\u05d5\u05dd \u05e8\u05d1\u05d9\u05e2\u05d9", // Wednesday
                    "\u05d9\u05d5\u05dd \u05d7\u05de\u05d9\u05e9\u05d9", // Thursday
                    "\u05d9\u05d5\u05dd \u05e9\u05d9\u05e9\u05d9", // Friday
                    "\u05e9\u05d1\u05ea" // Saturday
                }
            },
            { "DayAbbreviations",
                new String[] {
                    "\u05d0", // abb Sunday
                    "\u05d1", // abb Monday
                    "\u05d2", // abb Tuesday
                    "\u05d3", // abb Wednesday
                    "\u05d4", // abb Thursday
                    "\u05d5", // abb Friday
                    "\u05e9" // abb Saturday
                }
            },
            { "Eras",
                new String[] { // era strings
                    "\u05dc\u05e1\u05d4\"\u05e0",
                    "\u05dc\u05e4\u05e1\u05d4\"\u05e0"
                }
            },
            { "DateTimePatterns",
                new String[] {
                    "HH:mm:ss z", // full time pattern
                    "HH:mm:ss z", // long time pattern
                    "HH:mm:ss", // medium time pattern
                    "HH:mm", // short time pattern
                    "EEEE d MMMM yyyy", // full date pattern
                    "d MMMM yyyy", // long date pattern
                    "dd/MM/yyyy", // medium date pattern
                    "dd/MM/yy", // short date pattern
                    "{0} {1}" // date-time pattern
                }
            },
            { "CollationElements",
                /* for IW_IL, the following additions are needed: */
                 "& \u0361 = \u05c4 "
               + "& \u030d = \u0591 "
               + "; \u0592 "
               + "; \u0593 "
               + "; \u0594 "
               + "; \u0595 "
               + "; \u0596 "
               + "; \u0597 "
               + "; \u0598 "
               + "; \u0599 "
               + "& \u0301 = \u059a "
               + "& \u0300 = \u059b "
               + "& \u0307 = \u059c ; \u059d "
               + "& \u0302 = \u059e "
               + "& \u0308 = \u059f "
               + "& \u030c = \u05a0 "
               + "& \u0306 = \u05a1 "
               + "& \u0304 = \u05a3 ; \u05a4 "
               + "& \u0303 = \u05a5 "
               + "& \u030a = \u05a6 "
               + "& \u0328 = \u05a7 "
               + "& \u0327 = \u05a8 "
               + "& \u030b = \u05a9 "
               + "& \u0336 = \u05aa "
               + "& \u0337 = \u05ab "
               + "& \u0338 = \u05ac ; \u05ad ; \u05ae "
               + "; \u05af "
                // Points
               + "; \u05b0 "
               + "; \u05b1 "
               + "; \u05b2 "
               + "; \u05b3 "
               + "; \u05b4 "
               + "; \u05b5 "
               + "; \u05b6 "
               + "; \u05b7 "
               + "; \u05b8 "
               + "; \u05b9 "
               + "; \u05bb "
               + "; \u05bc "
               + "; \u05bd "
               + "; \u05bf "
               + "; \u05c0 "
               + "; \u05c1 "
               + "; \u05c2 "
                // Punctuations
               + "& \u00b5 < \u05be "
               + "< \u05c3 "
               + "< \u05f3 "
               + "< \u05f4 "
                // Hebrew letters sort after Z's
               + "& Z < \u05d0 "
               + "< \u05d1 "
               + "< \u05d2 "
               + "< \u05d3 "
               + "< \u05d4 "
               + "< \u05d5 "
               + "< \u05f0 "
               + "< \u05f1 "
               + "< \u05d6 "
               + "< \u05d7 "
               + "< \u05d8 "
               + "< \u05d9 "
               + "< \u05f2 "
               + "< \u05da , \u05db "
               + "< \u05dc "
               + "< \u05dd , \u05de "
               + "< \u05df , \u05e0 "
               + "< \u05e1 "
               + "< \u05e2 "
               + "< \u05e3 , \u05e4 "
               + "< \u05e5 , \u05e6 "
               + "< \u05e7 "
               + "< \u05e8 "
               + "< \u05e9 "
               + "< \u05ea "
            },
        };
    }
}
