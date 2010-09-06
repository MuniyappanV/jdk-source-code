/*
 * @(#)LocaleElements_el.java	1.20 03/12/19
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

public class LocaleElements_el extends ListResourceBundle {
    /**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return new Object[][] {
            { "Languages", // language names
                new String[][] {
                    { "el", "\u03b5\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ac" }
                }
            },
            { "Countries", // country names
                new String[][] {
                    { "GR", "\u0395\u03bb\u03bb\u03ac\u03b4\u03b1" }
                }
            },
            { "MonthNames",
                new String[] {
                    "\u0399\u03b1\u03bd\u03bf\u03c5\u03ac\u03c1\u03b9\u03bf\u03c2", // january
                    "\u03a6\u03b5\u03b2\u03c1\u03bf\u03c5\u03ac\u03c1\u03b9\u03bf\u03c2", // february
                    "\u039c\u03ac\u03c1\u03c4\u03b9\u03bf\u03c2", // march
                    "\u0391\u03c0\u03c1\u03af\u03bb\u03b9\u03bf\u03c2", // april
                    "\u039c\u03ac\u03ca\u03bf\u03c2", // may
                    "\u0399\u03bf\u03cd\u03bd\u03b9\u03bf\u03c2", // june
                    "\u0399\u03bf\u03cd\u03bb\u03b9\u03bf\u03c2", // july
                    "\u0391\u03cd\u03b3\u03bf\u03c5\u03c3\u03c4\u03bf\u03c2", // august
                    "\u03a3\u03b5\u03c0\u03c4\u03ad\u03bc\u03b2\u03c1\u03b9\u03bf\u03c2", // september
                    "\u039f\u03ba\u03c4\u03ce\u03b2\u03c1\u03b9\u03bf\u03c2", // october
                    "\u039d\u03bf\u03ad\u03bc\u03b2\u03c1\u03b9\u03bf\u03c2", // november
                    "\u0394\u03b5\u03ba\u03ad\u03bc\u03b2\u03c1\u03b9\u03bf\u03c2", // december
                    "" // month 13 if applicable
                }
            },
            { "MonthAbbreviations",
                new String[] {
                    "\u0399\u03b1\u03bd", // abb january
                    "\u03a6\u03b5\u03b2", // abb february
                    "\u039c\u03b1\u03c1", // abb march
                    "\u0391\u03c0\u03c1", // abb april
                    "\u039c\u03b1\u03ca", // abb may
                    "\u0399\u03bf\u03c5\u03bd", // abb june
                    "\u0399\u03bf\u03c5\u03bb", // abb july
                    "\u0391\u03c5\u03b3", // abb august
                    "\u03a3\u03b5\u03c0", // abb september
                    "\u039f\u03ba\u03c4", // abb october
                    "\u039d\u03bf\u03b5", // abb november
                    "\u0394\u03b5\u03ba", // abb december
                    "" // abb month 13 if applicable
                }
            },
            { "DayNames",
                new String[] {
                    "\u039a\u03c5\u03c1\u03b9\u03b1\u03ba\u03ae", // Sunday
                    "\u0394\u03b5\u03c5\u03c4\u03ad\u03c1\u03b1", // Monday
                    "\u03a4\u03c1\u03af\u03c4\u03b7", // Tuesday
                    "\u03a4\u03b5\u03c4\u03ac\u03c1\u03c4\u03b7", // Wednesday
                    "\u03a0\u03ad\u03bc\u03c0\u03c4\u03b7", // Thursday
                    "\u03a0\u03b1\u03c1\u03b1\u03c3\u03ba\u03b5\u03c5\u03ae", // Friday
                    "\u03a3\u03ac\u03b2\u03b2\u03b1\u03c4\u03bf" // Saturday
                }
            },
            { "DayAbbreviations",
                new String[] {
                    "\u039a\u03c5\u03c1", // abb Sunday
                    "\u0394\u03b5\u03c5", // abb Monday
                    "\u03a4\u03c1\u03b9", // abb Tuesday
                    "\u03a4\u03b5\u03c4", // abb Wednesday
                    "\u03a0\u03b5\u03bc", // abb Thursday
                    "\u03a0\u03b1\u03c1", // abb Friday
                    "\u03a3\u03b1\u03b2" // abb Saturday
                }
            },
            { "AmPmMarkers",
                new String[] {
                    "\u03c0\u03bc", // am marker
                    "\u03bc\u03bc" // pm marker
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
                    "h:mm:ss a z", // full time pattern
                    "h:mm:ss a z", // long time pattern
                    "h:mm:ss a", // medium time pattern
                    "h:mm a", // short time pattern
                    "EEEE, d MMMM yyyy", // full date pattern
                    "d MMMM yyyy", // long date pattern
                    "d MMM yyyy", // medium date pattern
                    "d/M/yyyy", // short date pattern
                    "{1} {0}" // date-time pattern
                }
            },
            { "DateTimeElements",
                new String[] {
                    "2", // first day of week
                    "1" // min days in first week
                }
            },
            { "CollationElements",
                "& \u0361 = \u0387 = \u03f3 " // ?? \u03f3 is letter yot
                // punctuations
                + "& \u00b5 "
                + "< \u0374 "        // upper numeral sign
                + "< \u0375 "        // lower numeral sign
                + "< \u037a "        // ypogegrammeni
                + "< \u037e "        // question mark
                + "< \u0384 "        // tonos
                + "< \u0385 "        // dialytika tonos
                // Greek letters sorts after Z's
                + "& Z < \u03b1 , \u0391 "  // alpha
                + "; \u03ac , \u0386 "  // alpha-tonos
                + "< \u03b2 , \u0392 "  // beta
                + "; \u03d0 "           // beta symbol
                + "< \u03b3 , \u0393 "  // gamma
                + "< \u03b4 , \u0394 "  // delta
                + "< \u03b5 , \u0395 "  // epsilon
                + "; \u03ad , \u0388 "  // epsilon-tonos
                + "< \u03b6 , \u0396 "  // zeta
                + "< \u03b7 , \u0397 "  // eta
                + "; \u03ae , \u0389 "  // eta-tonos
                + "< \u03b8 , \u0398 "  // theta
                + "; \u03d1 "           // theta-symbol
                + "< \u03b9 , \u0399 "  // iota
                + "; \u03af , \u038a "  // iota-tonos
                + "; \u03ca , \u03aa "  // iota-dialytika
                + "; \u0390 "           // iota-dialytika
                + "< \u03ba , \u039a "  // kappa
                + "; \u03f0 "           // kappa symbol
                + "< \u03bb , \u039b "  // lamda
                + "< \u03bc , \u039c "  // mu
                + "< \u03bd , \u039d "  // nu
                + "< \u03be , \u039e "  // xi
                + "< \u03bf , \u039f "  // omicron
                + "; \u03cc , \u038c "  // omicron-tonos
                + "< \u03c0 , \u03a0 "  // pi
                + "; \u03d6 < \u03c1 "  // pi-symbol
                + ", \u03a1 "           // rho
                + "; \u03f1 "           // rho-symbol
                + "< \u03c3 , \u03c2 "  // sigma(final)
                + ", \u03a3 "           // sigma
                + "; \u03f2 "           // sigma-symbol
                + "< \u03c4 , \u03a4 "  // tau
                + "< \u03c5 , \u03a5 "  // upsilon
                + "; \u03cd , \u038e "  // upsilon-tonos
                + "; \u03cb , \u03ab "  // upsilon-dialytika
                + "= \u03d4 "           // upsilon-diaeresis-hook
                + "; \u03b0 "           // upsilon-dialytika-tonos
                + "; \u03d2 "           // upsilon-hook symbol
                + "; \u03d3 "           // upsilon-acute-hook
                + "< \u03c6 , \u03a6 "  // phi
                + "; \u03d5 "           // phi-symbol
                + "< \u03c7 , \u03a7 "  // chi
                + "< \u03c8 , \u03a8 "  // psi
                + "< \u03c9 , \u03a9 "  // omega
                + "; \u03ce , \u038f "  // omega-tonos
                + ", \u03da , \u03dc "  // stigma, digamma
                + ", \u03de , \u03e0 "  // koppa, sampi
                + "< \u03e3 , \u03e2 "  // shei
                + "< \u03e5 , \u03e4 "  // fei
                + "< \u03e7 , \u03e6 "  // khei
                + "< \u03e9 , \u03e8 "  // hori
                + "< \u03eb , \u03ea "  // gangia
                + "< \u03ed , \u03ec "  // shima
                + "< \u03ef , \u03ee "  // dei

                + "& \u03bc = \u00b5 "  // Micro symbol sorts with mu
            }
        };
    }
}
