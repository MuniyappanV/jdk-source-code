/*
 * @(#)MultipleSubstSubtables.cpp	1.4 03/10/24 
 *
 * (C) Copyright IBM Corp. 1998-2003 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEGlyphFilter.h"
#include "OpenTypeTables.h"
#include "GlyphSubstitutionTables.h"
#include "MultipleSubstSubtables.h"
#include "GlyphIterator.h"
#include "LESwaps.h"

le_uint32 MultipleSubstitutionSubtable::process(GlyphIterator *glyphIterator, const LEGlyphFilter *filter) const
{
    LEGlyphID glyph = glyphIterator->getCurrGlyphID();
    le_int32 coverageIndex = getGlyphCoverage(glyph);
    le_uint16 seqCount = SWAPW(sequenceCount);

    if (coverageIndex >= 0 && coverageIndex < seqCount) {
        Offset sequenceTableOffset = SWAPW(sequenceTableOffsetArray[coverageIndex]);
        const SequenceTable *sequenceTable = (const SequenceTable *) ((char *) this + sequenceTableOffset);
        le_uint16 glyphCount = SWAPW(sequenceTable->glyphCount);

        if (glyphCount == 0) {
            glyphIterator->setCurrGlyphID(0xFFFF);
            return 1;
        } else if (glyphCount == 1) {
            TTGlyphID substitute = SWAPW(sequenceTable->substituteArray[0]);

            if (filter == NULL || filter->accept(LE_SET_GLYPH(glyph, substitute))) {
                glyphIterator->setCurrGlyphID(substitute);
            }

            return 1;
        } else {
            LEGlyphID *newGlyphs = glyphIterator->insertGlyphs(glyphCount);
            le_int32 insert = 0, direction = 1;

            if (glyphIterator->isRightToLeft()) {
                insert = glyphCount - 1;
                direction = -1;
            }

            for (le_int32 i = 0; i < glyphCount; i += 1) {
                TTGlyphID substitute = SWAPW(sequenceTable->substituteArray[i]);

                newGlyphs[insert] = LE_SET_GLYPH(glyph, substitute);
                insert += direction;
            }

            return 1;
        }
    }

    return 0;
}
