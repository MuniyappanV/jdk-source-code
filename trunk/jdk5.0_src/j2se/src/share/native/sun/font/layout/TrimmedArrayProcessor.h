/*
 * @(#)TrimmedArrayProcessor.h	1.8 01/10/09 
 *
 * (C) Copyright IBM Corp. 1998, 1999, 2000, 2001 - All Rights Reserved
 *
 */

#ifndef __TRIMMEDARRAYPROCESSOR_H
#define __TRIMMEDARRAYPROCESSOR_H

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "NonContextualGlyphSubst.h"
#include "NonContextualGlyphSubstProc.h"

class TrimmedArrayProcessor : public NonContextualGlyphSubstitutionProcessor
{
public:
    virtual void process(LEGlyphID *glyphs, le_int32 *charIndices, le_int32 glyphCount);

    TrimmedArrayProcessor(const MorphSubtableHeader *morphSubtableHeader);

    virtual ~TrimmedArrayProcessor();

private:
    TrimmedArrayProcessor();

protected:
    le_int16 firstGlyph;
    le_int16 lastGlyph;
    const TrimmedArrayLookupTable *trimmedArrayLookupTable;
};

#endif

