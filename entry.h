/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
*/

#ifndef ENTRY_H
#define ENTRY_H

#include <stdint.h>
#include <time.h>

// Struct for storing calender event info
#define INKY_ENTRY_MAXBYTES_NAME     128
#define INKY_ENTRY_MAXBYTES_TIME     128
#define INKY_ENTRY_MAXBYTES_LOCATION 128
typedef struct entry
{
    char name[INKY_ENTRY_MAXBYTES_NAME];
    char time[INKY_ENTRY_MAXBYTES_TIME];
    char location[INKY_ENTRY_MAXBYTES_LOCATION];
    time_t timeStamp;
    int8_t day = -1;
    int8_t sortTieBreak; //higher number, higher up display
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
