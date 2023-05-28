#ifndef ENTRY_H
#define ENTRY_H

#include <stdint.h>
#include <time.h>

// Struct for storing calender event info
typedef struct entry
{
    char name[128];
    char time[128];
    char location[128];
    time_t timeStamp;
    int8_t day = -1;
    int8_t bgColour;
    int8_t fgColour;
} entry_t;

#define INKY_EVENT_COLOUR_RANDOM (-1)
#define INKY_EVENT_COLOUR_BLACK    0
#define INKY_EVENT_COLOUR_WHITE    1
#define INKY_EVENT_COLOUR_GREEN    2
#define INKY_EVENT_COLOUR_BLUE     3
#define INKY_EVENT_COLOUR_RED      4
#define INKY_EVENT_COLOUR_YELLOW   5
#define INKY_EVENT_COLOUR_ORANGE   6

// Here we store calendar entries
extern int entriesNum;
#define MAX_ENTRIES 100
extern entry_t entries[MAX_ENTRIES];

//Set fg colour to an appropriate choice for bgColour
void entry_SetColour(entry_t *entry, uint8_t bgColour);

void resetEntries(void);
void SortEntries(void);

#endif 