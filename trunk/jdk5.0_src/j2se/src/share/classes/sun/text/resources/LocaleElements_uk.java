/*
 * @(#)LocaleElements_uk.java	1.19 03/12/19
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

public class LocaleElements_uk extends ListResourceBundle {
    /**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return new Object[][] {
            { "Languages", // language names
                new String[][] {
                    { "uk", "\u0443\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430" }
                }
            },
            { "Countries", // country names
                new String[][] {
                    { "UA", "\u0423\u043a\u0440\u0430\u0457\u043d\u0430" }
                }
            },
            { "MonthNames",
                new String[] {
                    "\u0441\u0456\u0447\u043d\u044f", // january
                    "\u043b\u044e\u0442\u043e\u0433\u043e", // february
                    "\u0431\u0435\u0440\u0435\u0437\u043d\u044f", // march
                    "\u043a\u0432\u0456\u0442\u043d\u044f", // april
                    "\u0442\u0440\u0430\u0432\u043d\u044f", // may
                    "\u0447\u0435\u0440\u0432\u043d\u044f", // june
                    "\u043b\u0438\u043f\u043d\u044f", // july
                    "\u0441\u0435\u0440\u043f\u043d\u044f", // august
                    "\u0432\u0435\u0440\u0435\u0441\u043d\u044f", // september
                    "\u0436\u043e\u0432\u0442\u043d\u044f", // october
                    "\u043b\u0438\u0441\u0442\u043e\u043f\u0430\u0434\u0430", // november
                    "\u0433\u0440\u0443\u0434\u043d\u044f", // december
                    "" // month 13 if applicable
                }
            },
            { "MonthAbbreviations",
                new String[] {
                    "\u0441\u0456\u0447", // abb january
                    "\u043b\u044e\u0442", // abb february
                    "\u0431\u0435\u0440", // abb march
                    "\u043a\u0432\u0456\u0442", // abb april
                    "\u0442\u0440\u0430\u0432", // abb may
                    "\u0447\u0435\u0440\u0432", // abb june
                    "\u043b\u0438\u043f", // abb july
                    "\u0441\u0435\u0440\u043f", // abb august
                    "\u0432\u0435\u0440", // abb september
                    "\u0436\u043e\u0432\u0442", // abb october
                    "\u043b\u0438\u0441\u0442", // abb november
                    "\u0433\u0440\u0443\u0434", // abb december
                    "" // abb month 13 if applicable
                }
            },
            { "DayNames",
                new String[] {
                    "\u043d\u0435\u0434\u0456\u043b\u044f", // Sunday
                    "\u043f\u043e\u043d\u0435\u0434\u0456\u043b\u043e\u043a", // Monday
                    "\u0432\u0456\u0432\u0442\u043e\u0440\u043e\u043a", // Tuesday
                    "\u0441\u0435\u0440\u0435\u0434\u0430", // Wednesday
                    "\u0447\u0435\u0442\u0432\u0435\u0440", // Thursday
                    "\u043f'\u044f\u0442\u043d\u0438\u0446\u044f", // Friday
                    "\u0441\u0443\u0431\u043e\u0442\u0430" // Saturday
                }
            },
            { "DayAbbreviations",
                new String[] {
                    "\u043d\u0434", // abb Sunday
                    "\u043f\u043d", // abb Monday
                    "\u0432\u0442", // abb Tuesday
                    "\u0441\u0440", // abb Wednesday
                    "\u0447\u0442", // abb Thursday
                    "\u043f\u0442", // abb Friday
                    "\u0441\u0431" // abb Saturday
                }
            },
            { "Eras",
                new String[] { // era strings
                    "\u0434\u043e \u043d.\u0435.",
                    "\u043f\u0456\u0441\u043b\u044f \u043d.\u0435."
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
                    "H:mm:ss z", // full time pattern
                    "H:mm:ss z", // long time pattern
                    "H:mm:ss", // medium time pattern
                    "H:mm", // short time pattern
                    "EEEE, d, MMMM yyyy", // full date pattern
                    "EEEE, d, MMMM yyyy", // long date pattern
                    "d/M/yyyy", // medium date pattern
                    "d/M/yy", // short date pattern
                    "{1} {0}" // date-time pattern
                }
            },
            { "CollationElements",
                /* for uk_UA, default plus the following */
                "& 9 < \u0482 " +       // thousand sign
                "& Z" +                 // Arabic script sorts after Z's
                "< \u0430 , \u0410" +   // a
                "< \u0431 , \u0411" +   // be
                "< \u0432 , \u0412" +   // ve
                "< \u0433 , \u0413" +   // ghe
                "; \u0491 , \u0490" +   // ghe-upturn
                "; \u0495 , \u0494" +   // ghe-mid-hook
                "; \u0453 , \u0403" +   // gje
                "; \u0493 , \u0492" +   // ghe-stroke
                "< \u0434 , \u0414" +   // de
                "< \u0452 , \u0402" +   // dje
                "< \u0435 , \u0415" +   // ie
                "; \u04bd , \u04bc" +   // che
                "; \u0451 , \u0401" +   // io
                "; \u04bf , \u04be" +   // che-descender
                "< \u0454 , \u0404" +   // uk ie
                "< \u0436 , \u0416" +   // zhe
                "; \u0497 , \u0496" +   // zhe-descender
                "; \u04c2 , \u04c1" +   // zhe-breve
                "< \u0437 , \u0417" +   // ze
                "; \u0499 , \u0498" +   // zh-descender
                "< \u0455 , \u0405" +   // dze
                "< \u0438 , \u0418" +   // i
                "< \u0456 , \u0406" +   // uk/bg i
                "; \u04c0 " +           // palochka
                "< \u0457 , \u0407" +   // uk yi
                "< \u0439 , \u0419" +   // short i
                "< \u0458 , \u0408" +   // je
                "< \u043a , \u041a" +   // ka
                "; \u049f , \u049e" +   // ka-stroke
                "; \u04c4 , \u04c3" +   // ka-hook
                "; \u049d , \u049c" +   // ka-vt-stroke
                "; \u04a1 , \u04a0" +   // bashkir-ka
                "; \u045c , \u040c" +   // kje
                "; \u049b , \u049a" +   // ka-descender
                "< \u043b , \u041b" +   // el
                "< \u0459 , \u0409" +   // lje
                "< \u043c , \u041c" +   // em
                "< \u043d , \u041d" +   // en
                "; \u0463 " +           // yat
                "; \u04a3 , \u04a2" +   // en-descender
                "; \u04a5 , \u04a4" +   // en-ghe
                "; \u04bb , \u04ba" +   // shha
                "; \u04c8 , \u04c7" +   // en-hook
                "< \u045a , \u040a" +   // nje
                "< \u043e , \u041e" +   // o
                "; \u04a9 , \u04a8" +   // ha
                "< \u043f , \u041f" +   // pe
                "; \u04a7 , \u04a6" +   // pe-mid-hook
                "< \u0440 , \u0420" +   // er
                "< \u0441 , \u0421" +   // es
                "; \u04ab , \u04aa" +   // es-descender
                "< \u0442 , \u0422" +   // te
                "; \u04ad , \u04ac" +   // te-descender
                "< \u045b , \u040b" +   // tshe
                "< \u0443 , \u0423" +   // u
                "; \u04af , \u04ae" +   // straight u
                "< \u045e , \u040e" +   // short u
                "< \u04b1 , \u04b0" +   // straight u-stroke
                "< \u0444 , \u0424" +   // ef
                "< \u0445 , \u0425" +   // ha
                "; \u04b3 , \u04b2" +   // ha-descender
                "< \u0446 , \u0426" +   // tse
                "; \u04b5 , \u04b4" +   // te tse
                "< \u0447 , \u0427" +   // che
                "; \u04b7 ; \u04b6" +   // che-descender
                "; \u04b9 , \u04b8" +   // che-vt-stroke
                "; \u04cc , \u04cb" +   // che
                "< \u045f , \u040f" +   // dzhe
                "< \u0448 , \u0428" +   // sha
                "< \u0449 , \u0429" +   // shcha
                "< \u044a , \u042a" +   // hard sign
                "< \u044b , \u042b" +   // yeru
                "< \u044d , \u042d" +   // e
                "< \u044e , \u042e" +   // yu
                "< \u044f , \u042f" +   // ya
                "< \u044c , \u042c" +   // soft sign
                "< \u0461 , \u0460" +   // omega
                "< \u0462 " +           // yat
                "< \u0465 , \u0464" +   // iotified e
                "< \u0467 , \u0466" +   // little yus
                "< \u0469 , \u0468" +   // iotified little yus
                "< \u046b , \u046a" +   // big yus
                "< \u046d , \u046c" +   // iotified big yus
                "< \u046f , \u046e" +   // ksi
                "< \u0471 , \u0470" +   // psi
                "< \u0473 , \u0472" +   // fita
                "< \u0475 , \u0474" +   // izhitsa
                "; \u0477 , \u0476" +   // izhitsa-double-grave
                "< \u0479 , \u0478" +   // uk
                "< \u047b , \u047a" +   // round omega
                "< \u047d , \u047c" +   // omega-titlo
                "< \u047f , \u047e" +   // ot
                "< \u0481 , \u0480"     // koppa
            }
        };
    }
}
