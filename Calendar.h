/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
*/

#ifndef CALENDAR_H
#define CALENDAR_H

#include "EventProcessing.h"

typedef struct {
    const char *url;
    const ProcessingRule_t *EventRules;
    int8_t eventColour;
    int8_t sortTieBreak; //higher number, higher up display
} Calendar_t;

typedef struct {
    Calendar_t *pCal;
    uint64_t calEvents = 0;
    uint64_t calRelevantEvents = 0;
} CalendarParsingContext_t;

//Sets the time period to find events for
// input: calendarStart (epoch time) - indicates the first day 
//                       (doesn't have to be midnight - first day is localtime day containing
//                        that time_t)
// input: numDays - number of days including the first day that events are relevant for
void setCalendarRange(time_t calendarStart, uint32_t numDays);

//returns 0 on error or number of chars (not including \0 added to buffer)
uint32_t  getTimeStringNow(char *buffer, size_t maxlen);

//Get String representing day some number of days after calendar start time
void getDateStringOffsetDays(char* timeStr, int32_t offsetDays, bool inclYear);

//context will get cast to a CalendarParsingContext_t *
//returns pointer to first unparsed data (or NULL on error)
char *parsePartialDataForEvents(char *rawData,  void *context);

uint64_t getRelevantEventCount(); //count of events relevant to calendar display
uint64_t getTotalEventCount();  //count of all events parsed
void resetEventStats();

#endif
