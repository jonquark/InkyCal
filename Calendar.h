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
    uint64_t totalEvents = 0;
    uint64_t totalEventsRelevant = 0;
} CalendarParsingContext_t;

extern int timeZone;

#define DAYS_SHOWN 3

void getDateStringOffset(char* timeStr, long offSet, bool inclYear);

//context will get cast to a CalendarParsingContext_t *
//returns pointer to first unparsed data (or NULL on error)
char *parsePartialDataForEvents(char *rawData, void *context);

#endif