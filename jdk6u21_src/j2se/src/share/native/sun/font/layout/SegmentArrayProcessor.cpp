/*
 * @(#)SegmentArrayProcessor.cpp	1.11 05/05/11
 *
 * (C) Copyright IBM Corp. 1998-2004 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "NonContextualGlyphSubst.h"
#include "NonContextualGlyphSubstProc.h"
#include "SegmentArrayProcessor.h"
#include "LEGlyphStorage.h"
#include "LESwaps.h"

SegmentArrayProcessor::SegmentArrayProcessor()
{
}

SegmentArrayProcessor::SegmentArrayProcessor(const MorphSubtableHeader *morphSubtableHeader)
  : NonContextualGlyphSubstitutionProcessor(morphSubtableHeader)
{
    const NonContextualGlyphSubstitutionHeader *header = (const NonContextualGlyphSubstitutionHeader *) morphSubtableHeader;

    segmentArrayLookupTable = (const SegmentArrayLookupTable *) &header->table;
}

SegmentArrayProcessor::~SegmentArrayProcessor()
{
}

void SegmentArrayProcessor::process(LEGlyphStorage &glyphStorage)
{
    const LookupSegment *segments = segmentArrayLookupTable->segments;
    le_int32 glyphCount = glyphStorage.getGlyphCount();
    le_int32 glyph;

    for (glyph = 0; glyph < glyphCount; glyph += 1) {
        LEGlyphID thisGlyph = glyphStorage[glyph];
        const LookupSegment *lookupSegment = segmentArrayLookupTable->lookupSegment(segments, thisGlyph);

        if (lookupSegment != NULL)  {
            TTGlyphID firstGlyph = SWAPW(lookupSegment->firstGlyph);
            le_int16  offset = SWAPW(lookupSegment->value);

            if (offset != 0) {
                TTGlyphID  *glyphArray = (TTGlyphID *) ((char *) subtableHeader + offset);
                TTGlyphID   newGlyph   = (TTGlyphID)SWAPW(glyphArray[LE_GET_GLYPH(thisGlyph) - firstGlyph]);
                
                glyphStorage[glyph] = LE_SET_GLYPH(thisGlyph, newGlyph);
            } 
        }
    }
}
 
