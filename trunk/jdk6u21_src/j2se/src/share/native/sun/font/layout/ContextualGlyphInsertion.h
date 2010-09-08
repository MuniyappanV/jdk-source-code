/*
 * @(#)ContextualGlyphInsertion.h	1.6 05/05/11
 *
 * (C) Copyright IBM Corp. 1998-2004 - All Rights Reserved
 *
 */

#ifndef __CONTEXTUALGLYPHINSERTION_H
#define __CONTEXTUALGLYPHINSERTION_H

#include "LETypes.h"
#include "LayoutTables.h"
#include "StateTables.h"
#include "MorphTables.h"
#include "MorphStateTables.h"

struct ContextualGlyphInsertionHeader : MorphStateTableHeader
{
};

enum ContextualGlyphInsertionFlags
{
    cgiSetMark                  = 0x8000,
    cgiDontAdvance              = 0x4000,
    cgiCurrentIsKashidaLike     = 0x2000,
    cgiMarkedIsKashidaLike      = 0x1000,
    cgiCurrentInsertBefore      = 0x0800,
    cgiMarkInsertBefore         = 0x0400,
    cgiCurrentInsertCountMask   = 0x03E0,
    cgiMarkedInsertCountMask    = 0x001F
};

struct LigatureSubstitutionStateEntry : StateEntry
{
    ByteOffset currentInsertionListOffset;
    ByteOffset markedInsertionListOffset;
};

#endif
