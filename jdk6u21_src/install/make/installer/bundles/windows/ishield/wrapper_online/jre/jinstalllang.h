/*
 * @(#)jinstalllang.h	1.6 05/11/17
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#ifndef __REGUTILS_LANG_H__
#define __REGUTILS_LANG_H__

#ifndef LGRPID_INSTALLED
#define LGRPID_INSTALLED 0x00000001
#endif
#ifndef LGRPID_WESTERN_EUROPE
#define LGRPID_WESTERN_EUROPE 0x0001   // Western Europe & U.S.
#endif
#ifndef LGRPID_CENTRAL_EUROPE
#define LGRPID_CENTRAL_EUROPE 0x0002
#endif
#ifndef LGRPID_BALTIC
#define LGRPID_BALTIC         0x0003
#endif
#ifndef LGRPID_GREEK
#define LGRPID_GREEK          0x0004
#endif
#ifndef LGRPID_CYRILLIC
#define LGRPID_CYRILLIC       0x0005
#endif
#ifndef LGRPID_TURKISH
#define LGRPID_TURKISH        0x0006
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1310)
typedef LPLONG LONG_PTR;
#endif

typedef DWORD LGRPID;
typedef BOOL    (CALLBACK *LANGUAGEGROUP_ENUMPROC)
  (LGRPID, LPTSTR, LPTSTR, DWORD, LONG_PTR);
typedef BOOL    (WINAPI *LPFNEnumSystemLanguageGroups)
  (LANGUAGEGROUP_ENUMPROC, DWORD, LONG_PTR);

// Language Groups that are supported by the default (non-international) JRE
// Used in Win2k / XP
// From http://www.microsoft.com/globaldev/win2k/setup/localsupport.asp
const LGRPID DEFAULT_LANGGRPS[] = {
  LGRPID_WESTERN_EUROPE,
  LGRPID_CENTRAL_EUROPE,
  LGRPID_BALTIC,
  LGRPID_GREEK,
  LGRPID_CYRILLIC,
  LGRPID_TURKISH
};

// Language IDs that are supported by the default (non-international) JRE
// Used in WinNT / 98 / ME
// From http://www.microsoft.com/globaldev/win2k/setup/localsupport.asp
const WORD DEFAULT_LANGIDS[] = { 

  // Western Europe and United States (1)
  MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_AFRIKAANS, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_BASQUE, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_CATALAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_DANISH, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_DUTCH, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_FAEROESE, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_FINNISH, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_FRENCH, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_GERMAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_ICELANDIC, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_INDONESIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_ITALIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_MALAY, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_NORWEGIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_PORTUGUESE, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_SPANISH, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_SWAHILI, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_SWEDISH, SUBLANG_NEUTRAL),

  // Central Europe (2)
  MAKELANGID(LANG_ALBANIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_CROATIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_CZECH, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_HUNGARIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_POLISH, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_ROMANIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_LATIN),
  MAKELANGID(LANG_SLOVAK, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_SLOVENIAN, SUBLANG_NEUTRAL),

  // Baltic (3)
  MAKELANGID(LANG_ESTONIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_LATVIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_LITHUANIAN, SUBLANG_NEUTRAL),

  // Greek (4)
  MAKELANGID(LANG_GREEK, SUBLANG_NEUTRAL),

  // Cyrillic (5)
  MAKELANGID(LANG_AZERI, SUBLANG_AZERI_CYRILLIC),
  MAKELANGID(LANG_BELARUSIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_BULGARIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_KAZAK, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_MACEDONIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_RUSSIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_CYRILLIC),
  MAKELANGID(LANG_TATAR, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_UKRAINIAN, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_UZBEK, SUBLANG_UZBEK_CYRILLIC),

  // Turkic (6)
  MAKELANGID(LANG_AZERI, SUBLANG_AZERI_LATIN),
  MAKELANGID(LANG_TURKISH, SUBLANG_NEUTRAL),
  MAKELANGID(LANG_UZBEK, SUBLANG_UZBEK_LATIN)
};

#endif // __REGUTILS_LANG_H__
