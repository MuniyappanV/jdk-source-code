/*
 * @(#)GlyphSubstitutionTables.cpp	1.15 03/10/24
 *
 * (C) Copyright IBM Corp. 1998-2003 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEGlyphFilter.h"
#include "OpenTypeTables.h"
#include "Lookups.h"
#include "GlyphDefinitionTables.h"
#include "GlyphPositionAdjustments.h"
#include "GlyphSubstitutionTables.h"
#include "GlyphSubstLookupProc.h"
#include "ScriptAndLanguage.h"
#include "LESwaps.h"

le_int32 GlyphSubstitutionTableHeader::process(LEGlyphID *&glyphs, const LETag **&glyphTags, 
    le_int32 *&charIndices, le_int32 glyphCount,
    le_bool rightToLeft, LETag scriptTag, LETag languageTag,
    const GlyphDefinitionTableHeader *glyphDefinitionTableHeader,
    const LEGlyphFilter *filter, const LETag *featureOrder) const
{
    GlyphSubstitutionLookupProcessor processor(this, scriptTag, languageTag, filter, featureOrder);

    return processor.process(glyphs, NULL, glyphTags, charIndices, glyphCount, 
        rightToLeft, glyphDefinitionTableHeader, NULL);
}

le_bool GlyphSubstitutionTableHeader::coversScript(LETag scriptTag) const
{
    const ScriptListTable *scriptListTable = (const ScriptListTable *) ((char *)this + SWAPW(scriptListOffset));

    return scriptListTable->findScript(scriptTag) != NULL;
}

le_bool GlyphSubstitutionTableHeader::coversScriptAndLanguage(LETag scriptTag, LETag languageTag) const
{
    const ScriptListTable *scriptListTable = (const ScriptListTable *) ((char *)this + SWAPW(scriptListOffset));

    return scriptListTable->findLanguage(scriptTag, languageTag, true) != NULL;
}

