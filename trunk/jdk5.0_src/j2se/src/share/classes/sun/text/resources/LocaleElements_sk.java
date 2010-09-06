/*
 * @(#)LocaleElements_sk.java	1.17 04/04/19
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

public class LocaleElements_sk extends ListResourceBundle {
    /**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return new Object[][] {
            { "Languages", // language names
                new String[][] {
                    { "sk", "Sloven\u010dina" }
                }
            },
            { "Countries", // country names
                new String[][] {
                    { "SK", "Slovensk\u00e1 republika" }
                }
            },
            { "MonthNames",
                new String[] {
                    "janu\u00e1r", // january
                    "febru\u00e1r", // february
                    "marec", // march
                    "apr\u00edl", // april
                    "m\u00e1j", // may
                    "j\u00fan", // june
                    "j\u00fal", // july
                    "august", // august
                    "september", // september
                    "okt\u00f3ber", // october
                    "november", // november
                    "december", // december
                    "" // month 13 if applicable
                }
            },
            { "MonthAbbreviations",
                new String[] {
                    "jan", // abb january
                    "feb", // abb february
                    "mar", // abb march
                    "apr", // abb april
                    "m\u00e1j", // abb may
                    "j\u00fan", // abb june
                    "j\u00fal", // abb july
                    "aug", // abb august
                    "sep", // abb september
                    "okt", // abb october
                    "nov", // abb november
                    "dec", // abb december
                    "" // abb month 13 if applicable
                }
            },
            { "DayNames",
                new String[] {
                    "Nede\u013ea", // Sunday
                    "Pondelok", // Monday
                    "Utorok", // Tuesday
                    "Streda", // Wednesday
                    "\u0160tvrtok", // Thursday
                    "Piatok", // Friday
                    "Sobota" // Saturday
                }
            },
            { "DayAbbreviations",
                new String[] {
                    "Ne", // abb Sunday
                    "Po", // abb Monday
                    "Ut", // abb Tuesday
                    "St", // abb Wednesday
                    "\u0160t", // abb Thursday
                    "Pi", // abb Friday
                    "So" // abb Saturday
                }
            },
            { "Eras",
                new String[] { // era strings
                    "pred n.l.",
                    "n.l."
                }
            },
            { "NumberElements",
                new String[] {
                    ",", // decimal separator
                    "\u00a0", // group (thousands) separator
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
                    "H:mm:ss z", // full time pattern
                    "H:mm:ss z", // long time pattern
                    "H:mm:ss", // medium time pattern
                    "H:mm", // short time pattern
                    "EEEE, yyyy, MMMM d", // full date pattern
                    "EEEE, yyyy, MMMM d", // long date pattern
                    "d.M.yyyy", // medium date pattern
                    "d.M.yyyy", // short date pattern
                    "{1} {0}" // date-time pattern
                }
            },
            { "CollationElements",
                /* for sk_SK, default sorting except for the following: */

                /* add d<stroke> between d and e. */
                /* add ch "ligature" between h and i */
                /* add l<stroke> between l and m. */
                /* add z<abovedot> after z.       */
                "& \u0361 ; \u0308 = \u030d "
                + "& A < a\u0308 , A\u0308 " // A < a-umlaut
                + "& C < c\u030c , C\u030c " // C < c-caron
                + "& D < \u0111, \u0110 "    // D < d-stroke
                + "& H < ch , cH , Ch , CH " // H < ch ligature
                + "& L < \u0142 , \u0141 "   // L < l-stroke
                + "& O < o\u0302 , O\u0302 " // oe < o-circumflex
                + "& R < r\u030c , R\u030c " // R < r-caron
                + "& S < s\u030c , S\u030c " // S < s-caron
                + "& Z < z\u030c , Z\u030c " // Z < z-caron
                + "< z\u0307 , Z\u0307 "     // z-dot-above
            }
        };
    }
}
