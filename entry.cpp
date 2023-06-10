/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
*/
#include <string.h>
#include <stdlib.h>

#include "entry.h"
#include "InkyCalInternal.h"

// Here we store calendar entries
int entriesNum = 0;
entry_t entries[MAX_ENTRIES];

void entry_SetColour(entry_t *entry, uint8_t bgColour)
{
    entry->bgColour = bgColour;

    if (   bgColour == INKY_EVENT_COLOUR_WHITE 
        || bgColour == INKY_EVENT_COLOUR_YELLOW)
    {
        entry->fgColour = INKY_EVENT_COLOUR_BLACK;
    }
    else
    {
        entry->fgColour = INKY_EVENT_COLOUR_WHITE;
    }
}

void resetEntries(void)
{
    // reset count
    entriesNum = 0;
}



// Struct event comparison function, by timestamp then sortTie 
//used for qsort below
int cmp(const void *a, const void *b)
{
    entry_t *entryA = (entry_t *)a;
    entry_t *entryB = (entry_t *)b;
    int sortresult;

    if (entryA->timeStamp != entryB->timeStamp)
    {
        //earlier timestamp higher up display
        sortresult = entryA->timeStamp - entryB->timeStamp;
    }
    else if (entryA->sortTieBreak != entryB->sortTieBreak)
    {
        //sort order for tie break is opposite to timestamp - higher sort tie is higher up display
        sortresult = (int)entryB->sortTieBreak - (int)entryA->sortTieBreak;
    }
    else
    {
        //anything to try and ensure consistent order so entries don't "jumble" on refresh
        sortresult = strcmp(entryA->name, entryB->name);
    }

    return sortresult;
}

void SortEntries(void)
{
    // Sort entries by time
    qsort(entries, entriesNum, sizeof(entry_t), cmp);
}
