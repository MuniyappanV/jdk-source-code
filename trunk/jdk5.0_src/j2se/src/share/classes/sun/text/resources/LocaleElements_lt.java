/*
 * @(#)LocaleElements_lt.java	1.17 03/12/19
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

public class LocaleElements_lt extends ListResourceBundle {
    /**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return new Object[][] {
            { "Languages", // language names
                new String[][] {
                    { "lt", "Lietuvi\u0173" }
                }
            },
            { "Countries", // country names
                new String[][] {
                    { "LT", "Lietuva" }
                }
            },
            { "MonthNames",
                new String[] {
                    "Sausio", // january
                    "Vasario", // february
                    "Kovo", // march
                    "Baland\u017eio", // april
                    "Gegu\u017e\u0117s", // may
                    "Bir\u017eelio", // june
                    "Liepos", // july
                    "Rugpj\u016b\u010dio", // august
                    "Rugs\u0117jo", // september
                    "Spalio", // october
                    "Lapkri\u010dio", // november
                    "Gruod\u017eio", // december
                    "" // month 13 if applicable
                }
            },
            { "MonthAbbreviations",
                new String[] {
                    "Sau", // abb january
                    "Vas", // abb february
                    "Kov", // abb march
                    "Bal", // abb april
                    "Geg", // abb may
                    "Bir", // abb june
                    "Lie", // abb july
                    "Rgp", // abb august
                    "Rgs", // abb september
                    "Spa", // abb october
                    "Lap", // abb november
                    "Grd", // abb december
                    "" // abb month 13 if applicable
                }
            },
            { "DayNames",
                new String[] {
                    "Sekmadienis", // Sunday
                    "Pirmadienis", // Monday
                    "Antradienis", // Tuesday
                    "Tre\u010diadienis", // Wednesday
                    "Ketvirtadienis", // Thursday
                    "Penktadienis", // Friday
                    "\u0160e\u0161tadienis" // Saturday
                }
            },
            { "DayAbbreviations",
                new String[] {
                    "Sk", // abb Sunday
                    "Pr", // abb Monday
                    "An", // abb Tuesday
                    "Tr", // abb Wednesday
                    "Kt", // abb Thursday
                    "Pn", // abb Friday
                    "\u0160t" // abb Saturday
                }
            },
            { "Eras",
                new String[] { // era strings
                    "pr.Kr.",
                    "po.Kr."
                }
            },
            { "NumberElements",
                new String[] {
                    ",", // decimal separator
                    ".", // group (thousands) separator
                    ";", // list separator
                    "%", // percent sign
                    "0", // native 0 digit
                    "#", // pattern digit
                    "-", // minus sign
                    "E", // exponential
                    "\u2030", // per mille
                    "\u221e", // infinity
                    "\ufffd" // NaN
                }
            },
            { "DateTimePatterns",
                new String[] {
                    "HH.mm.ss z", // full time pattern
                    "HH.mm.ss z", // long time pattern
                    "HH.mm.ss", // medium time pattern
                    "HH.mm", // short time pattern
                    "EEEE, yyyy, MMMM d", // full date pattern
                    "EEEE, yyyy, MMMM d", // long date pattern
                    "yyyy.M.d", // medium date pattern
                    "yy.M.d", // short date pattern
                    "{1} {0}" // date-time pattern
                }
            },
            { "CollationElements",
                /* for LT_LT, accents sorted backwards plus the following: */
                "@" +                                     // tal : french secondary
                "& C < c\u030c , C\u030c " +              // nt : open-o < c-caron
                "& I ; y = \u0131 , Y = \u0130 " +        // nt : i is equivalent to y
                "& S < s\u030c , S\u030c " +              // nt : long-s < s-caron
                "& X < y\u0301, Y\u0301 "+                // nt : x < y-acute
                "< y\u0302 , Y\u0302 < y\u0308, Y\u0308 " + // nt : y-circumflex < y-umlaut
                "& Z < z\u030c , Z\u030c "                // nt : ezh-tail < z-caron
            }
        };
    }
}
