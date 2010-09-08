/*
 * @(#)LigatureSubstitution.h	1.7 05/05/11
 *
 * (C) Copyright IBM Corp. 1998-2004 - All Rights Reserved
 *
 */

#ifndef __LIGATURESUBSTITUTION_H
#define __LIGATURESUBSTITUTION_H

#include "LETypes.h"
#include "LayoutTables.h"
#include "StateTables.h"
#include "MorphTables.h"
#include "MorphStateTables.h"

struct LigatureSubstitutionHeader : MorphStateTableHeader
{
    ByteOffset ligatureActionTableOffset;
    ByteOffset componentTableOffset;
    ByteOffset ligatureTableOffset;
};

enum LigatureSubstitutionFlags
{
    lsfSetComponent     = 0x8000,
    lsfDontAdvance      = 0x4000,
    lsfActionOffsetMask = 0x3FFF
};

struct LigatureSubstitutionStateEntry : StateEntry
{
};

typedef le_uint32 LigatureActionEntry;

enum LigatureActionFlags
{
    lafLast                 = 0x80000000,
    lafStore                = 0x40000000,
    lafComponentOffsetMask  = 0x3FFFFFFF
};

#endif
