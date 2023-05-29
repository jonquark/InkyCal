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



// Struct event comparison function, by timestamp, used for qsort later on
int cmp(const void *a, const void *b)
{
    entry_t *entryA = (entry_t *)a;
    entry_t *entryB = (entry_t *)b;

    return (entryA->timeStamp - entryB->timeStamp);
}

void SortEntries(void)
{
    // Sort entries by time
    qsort(entries, entriesNum, sizeof(entry_t), cmp);
}
