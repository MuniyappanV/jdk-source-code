/*
 * @(#)SubtableProcessor.cpp	1.7 01/10/09 
 *
 * (C) Copyright IBM Corp. 1998, 1999, 2000, 2001 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "LESwaps.h"

SubtableProcessor::SubtableProcessor()
{
}

SubtableProcessor::SubtableProcessor(const MorphSubtableHeader *morphSubtableHeader)
{
    subtableHeader = morphSubtableHeader;

    length = SWAPW(subtableHeader->length);
    coverage = SWAPW(subtableHeader->coverage);
    subtableFeatures = SWAPL(subtableHeader->subtableFeatures);
}

SubtableProcessor::~SubtableProcessor()
{
}

