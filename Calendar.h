#ifndef CALENDAR_H
#define CALENDAR_H

#include "EventProcessing.h"

typedef struct {
    const char *url;
    const ProcessingRule_t *EventRules;
    int8_t eventColour;
} Calendar_t;

typedef struct {
    Calendar_t *pCal;
    uint64_t calEvents = 0;
    uint64_t calRelevantEvents = 0;
} CalendarParsingContext_t;

extern int timeZone;

#define DAYS_SHOWN 3

//returns 0 on error or number of types (not including \0 added to buffer)
uint32_t  getTimeStringNow(char *buffer, size_t maxlen);

void getDateStringOffset(char* timeStr, long offSet, bool inclYear);

//context will get cast to a CalendarParsingContext_t *
//returns pointer to first unparsed data (or NULL on error)
char *parsePartialDataForEvents(char *rawData, void *context);


uint64_t getRelevantEventCount(); //count of events relevant to calendar display
uint64_t getTotalEventCount();  //count of all events parsed

#endif