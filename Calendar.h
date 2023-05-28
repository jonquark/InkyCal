#ifndef CALENDAR_H
#define CALENDAR_H

#include "EventProcessing.h"

typedef struct {
    const char *url;
    const ProcessingRule_t *EventRules;
    int8_t eventColour;
} Calendar_t;

extern int timeZone;

#define DAYS_SHOWN 3

void getDateStringOffset(char* timeStr, long offSet, bool inclYear);

void parseDataForEvents(Calendar_t *pCal, char *rawData);

#endif