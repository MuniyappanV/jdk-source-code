/*
 * @(#)GlyphSubstLookupProc.h	1.2 02/12/11
 *
 * (C) Copyright IBM Corp. 1998, 1999, 2000, 2001 - All Rights Reserved
 *
 */

#ifndef __GLYPHSUBSTITUTIONLOOKUPPROCESSOR_H
#define __GLYPHSUBSTITUTIONLOOKUPPROCESSOR_H

#include "LETypes.h"
#include "LEGlyphFilter.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "Lookups.h"
#include "Features.h"
#include "GlyphDefinitionTables.h"
#include "GlyphSubstitutionTables.h"
#include "GlyphIterator.h"
#include "LookupProcessor.h"

class GlyphSubstitutionLookupProcessor : public LookupProcessor
{
public:
    GlyphSubstitutionLookupProcessor(const GlyphSubstitutionTableHeader *glyphSubstitutionTableHeader,
        LETag scriptTag, LETag languageTag, const LEGlyphFilter *filter = NULL, const LETag *featureOrder = NULL);

    virtual ~GlyphSubstitutionLookupProcessor();

    virtual le_uint32 applySubtable(const LookupSubtable *lookupSubtable, le_uint16 lookupType, GlyphIterator *glyphIterator,
        const LEFontInstance *fontInstance) const;

protected:
    GlyphSubstitutionLookupProcessor();

private:
    const LEGlyphFilter *fFilter;

    GlyphSubstitutionLookupProcessor(const GlyphSubstitutionLookupProcessor &other); // forbid copying of this class
    GlyphSubstitutionLookupProcessor &operator=(const GlyphSubstitutionLookupProcessor &other); // forbid copying of this class
};

#endif
