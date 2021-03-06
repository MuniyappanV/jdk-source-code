/*
 * @(#)MarkToBasePosnSubtables.cpp	1.6 04/01/14
 *
 * (C) Copyright IBM Corp. 1998-2003 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "AnchorTables.h"
#include "MarkArrays.h"
#include "GlyphPositioningTables.h"
#include "AttachmentPosnSubtables.h"
#include "MarkToBasePosnSubtables.h"
#include "GlyphIterator.h"
#include "LESwaps.h"

LEGlyphID MarkToBasePositioningSubtable::findBaseGlyph(GlyphIterator *glyphIterator) const
{
    if (glyphIterator->prev()) {
        return glyphIterator->getCurrGlyphID();
    }

    return 0xFFFF;
}

le_int32 MarkToBasePositioningSubtable::process(GlyphIterator *glyphIterator, const LEFontInstance *fontInstance) const
{
    LEGlyphID markGlyph = glyphIterator->getCurrGlyphID();
    le_int32 markCoverage = getGlyphCoverage((LEGlyphID) markGlyph);

    if (markCoverage < 0) {
        // markGlyph isn't a covered mark glyph
        return 0;
    }

    LEPoint markAnchor;
    const MarkArray *markArray = (const MarkArray *) ((char *) this + SWAPW(markArrayOffset));
    le_int32 markClass = markArray->getMarkClass(markGlyph, markCoverage, fontInstance, markAnchor);
    le_uint16 mcCount = SWAPW(classCount);

    if (markClass < 0 || markClass >= mcCount) {
        // markGlyph isn't in the mark array or its
        // mark class is too big. The table is mal-formed!
        return 0;
    }

    // FIXME: We probably don't want to find a base glyph before a previous ligature... 
    GlyphIterator baseIterator(*glyphIterator, (le_uint16) (lfIgnoreMarks /*| lfIgnoreLigatures*/));
    LEGlyphID baseGlyph = findBaseGlyph(&baseIterator);
    le_int32 baseCoverage = getBaseCoverage((LEGlyphID) baseGlyph);
    const BaseArray *baseArray = (const BaseArray *) ((char *) this + SWAPW(baseArrayOffset));
    le_uint16 baseCount = SWAPW(baseArray->baseRecordCount);

    if (baseCoverage < 0 || baseCoverage >= baseCount) {
        // The base glyph isn't covered, or the coverage
        // index is too big. The latter means that the
        // table is mal-formed...
        return 0;
    }

    const BaseRecord *baseRecord = &baseArray->baseRecordArray[baseCoverage * mcCount];
    Offset anchorTableOffset = SWAPW(baseRecord->baseAnchorTableOffsetArray[markClass]);
    const AnchorTable *anchorTable = (const AnchorTable *) ((char *) baseArray + anchorTableOffset);
    LEPoint baseAnchor, markAdvance, pixels;

    if (anchorTableOffset == 0) {
        // this means the table is mal-formed...
        glyphIterator->setCurrGlyphBaseOffset(baseIterator.getCurrStreamPosition());
        return 0;
    }

    anchorTable->getAnchor(baseGlyph, fontInstance, baseAnchor);

    fontInstance->getGlyphAdvance(markGlyph, pixels);
    fontInstance->pixelsToUnits(pixels, markAdvance);

    float anchorDiffX = baseAnchor.fX - markAnchor.fX;
    float anchorDiffY = baseAnchor.fY - markAnchor.fY;

    glyphIterator->setCurrGlyphBaseOffset(baseIterator.getCurrStreamPosition());

    if (glyphIterator->isRightToLeft()) {
        // flip advance to local coordinate system
        glyphIterator->adjustCurrGlyphPositionAdjustment(anchorDiffX, anchorDiffY, -markAdvance.fX, markAdvance.fY);
    } else {
        LEPoint baseAdvance;

        fontInstance->getGlyphAdvance(baseGlyph, pixels);
        fontInstance->pixelsToUnits(pixels, baseAdvance);

	// flip advances to local coordinate system
        glyphIterator->adjustCurrGlyphPositionAdjustment(anchorDiffX - baseAdvance.fX, anchorDiffY + baseAdvance.fY, -markAdvance.fX, markAdvance.fY);
    }

    return 1;
}
