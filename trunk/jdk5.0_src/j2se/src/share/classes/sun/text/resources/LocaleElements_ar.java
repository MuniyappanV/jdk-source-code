/*
 * @(#)LocaleElements_ar.java	1.21 03/12/19
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

public class LocaleElements_ar extends ListResourceBundle {
    /**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return new Object[][] {
            { "Languages", // language names
                new String[][] {
                    { "ar", "\u0627\u0644\u0639\u0631\u0628\u064a\u0629" }
                }
            },
            { "Countries", // country names
                new String[][] {
                    { "EG", "\u0645\u0635\u0631" },
                    { "DZ", "\u0627\u0644\u062c\u0632\u0627\u0626\u0631" },
                    { "BH", "\u0627\u0644\u0628\u062d\u0631\u064a\u0646" },
                    { "IQ", "\u0627\u0644\u0639\u0631\u0627\u0642" },
                    { "JO", "\u0627\u0644\u0623\u0631\u062f\u0646" },
                    { "KW", "\u0627\u0644\u0643\u0648\u064a\u062a" },
                    { "LB", "\u0644\u0628\u0646\u0627\u0646" },
                    { "LY", "\u0644\u064a\u0628\u064a\u0627" },
                    { "MA", "\u0627\u0644\u0645\u063a\u0631\u0628" },
                    { "OM", "\u0633\u0644\u0637\u0646\u0629\u0020\u0639\u0645\u0627\u0646" },
                    { "QA", "\u0642\u0637\u0631" },
                    { "SA", "\u0627\u0644\u0633\u0639\u0648\u062f\u064a\u0629" },
                    { "SD", "\u0627\u0644\u0633\u0648\u062f\u0627\u0646" },
                    { "SY", "\u0633\u0648\u0631\u064a\u0627" },
                    { "TN", "\u062a\u0648\u0646\u0633" },
                    { "AE", "\u0627\u0644\u0625\u0645\u0627\u0631\u0627\u062a" },
                    { "YE", "\u0627\u0644\u064a\u0645\u0646" }
                }
            },
            { "MonthNames",
                new String[] {
                    "\u064a\u0646\u0627\u064a\u0631", // january
                    "\u0641\u0628\u0631\u0627\u064a\u0631", // february
                    "\u0645\u0627\u0631\u0633", // march
                    "\u0623\u0628\u0631\u064a\u0644", // april
                    "\u0645\u0627\u064a\u0648", // may
                    "\u064a\u0648\u0646\u064a\u0648", // june
                    "\u064a\u0648\u0644\u064a\u0648", // july
                    "\u0623\u063a\u0633\u0637\u0633", // august
                    "\u0633\u0628\u062a\u0645\u0628\u0631", // september
                    "\u0623\u0643\u062a\u0648\u0628\u0631", // october
                    "\u0646\u0648\u0641\u0645\u0628\u0631", // november
                    "\u062f\u064a\u0633\u0645\u0628\u0631", // december
                    "" // month 13 if applicable
                }
            },
            { "MonthAbbreviations",
                new String[] {
                    "\u064a\u0646\u0627", // abb january
                    "\u0641\u0628\u0631", // abb february
                    "\u0645\u0627\u0631", // abb march
                    "\u0623\u0628\u0631", // abb april
                    "\u0645\u0627\u064a", // abb may
                    "\u064a\u0648\u0646", // abb june
                    "\u064a\u0648\u0644", // abb july
                    "\u0623\u063a\u0633", // abb august
                    "\u0633\u0628\u062a", // abb september
                    "\u0623\u0643\u062a", // abb october
                    "\u0646\u0648\u0641", // abb november
                    "\u062f\u064a\u0633", // abb december
                    "" // abb month 13 if applicable
                }
            },
            { "DayNames",
                new String[] {
                    "\u0627\u0644\u0623\u062d\u062f", // Sunday
                    "\u0627\u0644\u0627\u062b\u0646\u064a\u0646", // Monday
                    "\u0627\u0644\u062b\u0644\u0627\u062b\u0627\u0621", // Tuesday
                    "\u0627\u0644\u0623\u0631\u0628\u0639\u0627\u0621", // Wednesday
                    "\u0627\u0644\u062e\u0645\u064a\u0633", // Thursday
                    "\u0627\u0644\u062c\u0645\u0639\u0629", // Friday
                    "\u0627\u0644\u0633\u0628\u062a" // Saturday
                }
            },
            { "DayAbbreviations",
                new String[] {
                    "\u062d", // abb Sunday
                    "\u0646", // abb Monday
                    "\u062b", // abb Tuesday
                    "\u0631", // abb Wednesday
                    "\u062e", // abb Thursday
                    "\u062c", // abb Friday
                    "\u0633" // abb Saturday
                }
            },
            { "AmPmMarkers",
                new String[] {
                    "\u0635", // am marker
                    "\u0645" // pm marker
                }
            },
            { "Eras",
                new String[] { // era strings
                    "\u0642.\u0645",
                    "\u0645"
                }
            },
            { "NumberPatterns",
                new String[] {
                    "#,##0.###;#,##0.###-", // decimal pattern
                    "\u00A4 #,##0.###;\u00A4 #,##0.###-", // currency pattern
                    "#,##0%" // percent pattern
                }
            },
            { "DateTimePatterns",
                new String[] {
                    "z hh:mm:ss a", // full time pattern
                    "z hh:mm:ss a", // long time pattern
                    "hh:mm:ss a", // medium time pattern
                    "hh:mm a", // short time pattern
                    "dd MMMM, yyyy", // full date pattern
                    "dd MMMM, yyyy", // long date pattern
                    "dd/MM/yyyy", // medium date pattern
                    "dd/MM/yy", // short date pattern
                    "{1} {0}" // date-time pattern
                }
            },
            { "DateTimeElements",
                new String[] {
                    "7", // first day of week
                    "1" // min days in first week
                }
            },
            { "CollationElements",
                // for ar, the following additions are needed:
               "& \u0361 = \u0640"
               + "= \u064b"
               + "= \u064c"
               + "= \u064d"
               + "= \u064e"
               + "= \u064f"
               + "= \u0650"
               + "= \u0652"
               + "= \u066d"
               + "= \u06d6"
               + "= \u06d7"
               + "= \u06d8"
               + "= \u06d9"
               + "= \u06da"
               + "= \u06db"
               + "= \u06dc"
               + "= \u06dd"
               + "= \u06de"
               + "= \u06df"
               + "= \u06e0"
               + "= \u06e1"
               + "= \u06e2"
               + "= \u06e3"
               + "= \u06e4"
               + "= \u06e5"
               + "= \u06e6"
               + "= \u06e7"
               + "= \u06e8"
               + "= \u06e9"
               + "= \u06ea"
               + "= \u06eb"
               + "= \u06ec"
               + "= \u06ed"
                // Numerics
               + "& 0 < \u0660 < \u06f0"       // 0
               + "& 1 < \u0661 < \u06f1"       // 1
               + "& 2 < \u0662 < \u06f2"       // 2
               + "& 3 < \u0663 < \u06f3"       // 3
               + "& 4 < \u0664 < \u06f4"       // 4
               + "& 5 < \u0665 < \u06f5"       // 5
               + "& 6 < \u0666 < \u06f6"       // 6
               + "& 7 < \u0667 < \u06f7"       // 7
               + "& 8 < \u0668 < \u06f8"       // 8
               + "& 9 < \u0669 < \u06f9"       // 9
                // Punctuations
               + "& \u00b5 < \u060c"  // retroflex click < arabic comma
               + "< \u061b"           // ar semicolon
               + "< \u061f"           // ar question mark
               + "< \u066a"           // ar percent sign
               + "< \u066b"           // ar decimal separator
               + "< \u066c"           // ar thousand separator
               + "< \u06d4"           // ar full stop
                // Arabic script sorts after Z's
               + "&  Z <  \u0621"
               + "; \u0622"
               + "; \u0623"
               + "; \u0624"
               + "; \u0625"
               + "; \u0626"
               + "< \u0627"
               + "< \u0628"
               + "< \u067e"
               + "< \u0629"
               + "= \u062a"
               + "< \u062b"
               + "< \u062c"
               + "< \u0686"
               + "< \u062d"
               + "< \u062e"
               + "< \u062f"
               + "< \u0630"
               + "< \u0631"
               + "< \u0632"
               + "< \u0698"
               + "< \u0633"
               + "< \u0634"
               + "< \u0635"
               + "< \u0636"
               + "< \u0637"
               + "< \u0638"
               + "< \u0639"
               + "< \u063a"
               + "< \u0641"
               + "< \u0642"
               + "< \u0643"
               + "< \u06af"
               + "< \u0644"
               + "< \u0645"
               + "< \u0646"
               + "< \u0647"
               + "< \u0648"
               + "< \u0649"
               + "; \u064a"
               + "< \u0670"
               + "< \u0671"
               + "< \u0672"
               + "< \u0673"
               + "< \u0674"
               + "< \u0675"
               + "< \u0676"
               + "< \u0677"
               + "< \u0678"
               + "< \u0679"
               + "< \u067a"
               + "< \u067b"
               + "< \u067c"
               + "< \u067d"
               + "< \u067f"
               + "< \u0680"
               + "< \u0681"
               + "< \u0682"
               + "< \u0683"
               + "< \u0684"
               + "< \u0685"
               + "< \u0687"
               + "< \u0688"
               + "< \u0689"
               + "< \u068a"
               + "< \u068b"
               + "< \u068c"
               + "< \u068d"
               + "< \u068e"
               + "< \u068f"
               + "< \u0690"
               + "< \u0691"
               + "< \u0692"
               + "< \u0693"
               + "< \u0694"
               + "< \u0695"
               + "< \u0696"
               + "< \u0697"
               + "< \u0699"
               + "< \u069a"
               + "< \u069b"
               + "< \u069c"
               + "< \u069d"
               + "< \u069e"
               + "< \u069f"
               + "< \u06a0"
               + "< \u06a1"
               + "< \u06a2"
               + "< \u06a3"
               + "< \u06a4"
               + "< \u06a5"
               + "< \u06a6"
               + "< \u06a7"
               + "< \u06a8"
               + "< \u06a9"
               + "< \u06aa"
               + "< \u06ab"
               + "< \u06ac"
               + "< \u06ad"
               + "< \u06ae"
               + "< \u06b0"
               + "< \u06b1"
               + "< \u06b2"
               + "< \u06b3"
               + "< \u06b4"
               + "< \u06b5"
               + "< \u06b6"
               + "< \u06b7"
               + "< \u06ba"
               + "< \u06bb"
               + "< \u06bc"
               + "< \u06bd"
               + "< \u06be"
               + "< \u06c0"
               + "< \u06c1"
               + "< \u06c2"
               + "< \u06c3"
               + "< \u06c4"
               + "< \u06c5"
               + "< \u06c6"
               + "< \u06c7"
               + "< \u06c8"
               + "< \u06c9"
               + "< \u06ca"
               + "< \u06cb"
               + "< \u06cc"
               + "< \u06cd"
               + "< \u06ce"
               + "< \u06d0"
               + "< \u06d1"
               + "< \u06d2"
               + "< \u06d3"
               + "< \u06d5"
               + "< \u0651"
            },
        };
    }
}
