/*
 * @(#)KernTable.h	1.4 05/08/24
 *
 * (C) Copyright IBM Corp. 2004-2005 - All Rights Reserved
 *
 */

#ifndef __KERNTABLE_H
#define __KERNTABLE_H

#ifndef __LETYPES_H
#include "LETypes.h"
#endif

#include "LETypes.h"

#include <stdio.h>

struct PairInfo;
class  LEFontInstance;
class  LEGlyphStorage;

/**
 * Windows type 0 kerning table support only for now.
 */
class U_LAYOUT_API KernTable
{
 private:
  le_uint16 coverage;
  le_uint16 nPairs;
  const PairInfo* pairs;
  const LEFontInstance* font;
  le_uint16 searchRange;
  le_uint16 entrySelector;
  le_uint16 rangeShift;

 public:
  KernTable(const LEFontInstance* font, const void* tableData);

  /*
   * Process the glyph positions.
   */
  void process(LEGlyphStorage& storage);
};

#endif

