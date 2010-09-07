/*
 * @(#)AttachmentPosnSubtables.h	1.2 01/10/09 
 *
 * (C) Copyright IBM Corp. 1998, 1999, 2000, 2001 - All Rights Reserved
 *
 */

#ifndef __ATTACHMENTPOSITIONINGSUBTABLES_H
#define __ATTACHMENTPOSITIONINGSUBTABLES_H

#include "LETypes.h"
#include "OpenTypeTables.h"
#include "GlyphPositioningTables.h"
#include "ValueRecords.h"
#include "GlyphIterator.h"

struct AttachmentPositioningSubtable : GlyphPositioningSubtable
{
    Offset    baseCoverageTableOffset;
    le_uint16 classCount;
    Offset    markArrayOffset;
    Offset    baseArrayOffset;

    le_int32  getBaseCoverage(LEGlyphID baseGlyphId) const;
    le_uint32 process(GlyphIterator *glyphIterator) const;
};

inline le_int32 AttachmentPositioningSubtable::getBaseCoverage(LEGlyphID baseGlyphID) const
{
    return getGlyphCoverage(baseCoverageTableOffset, baseGlyphID);
}

#endif

