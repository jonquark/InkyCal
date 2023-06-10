#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "utils/test_utils.h"
#include "Calendar.h"

typedef struct {
    const char *calStartYYYYMMDD;
    const char *path;
    const char *desc;
    uint32_t relevantEventCount;
    uint32_t  totalEventCount;
    const char *event0CheckSummary;
    const char *event0CheckLocation;
} calendarFragment_t;

//In Calendar.cpp but not exposed in the headwer:
time_t convertYYYYMMDDtoEpochTime(const char *dayYYYYMMDD);

int testCalFragments(void)
{
    Calendar_t testCal = {};
    CalendarParsingContext_t calParsingContext = { &testCal };

    calendarFragment_t frags[] = {
        {"20221108", "resources/calfrag_simple", "Single event - on day before calendar start", 0, 1, NULL, NULL},
        {"20221106", "resources/calfrag_simple", "Single event - no recurring/not all day but for relevant day", 1, 1, "Tickets available for Worthy Players panto?", ""},
        {"20230722", "resources/calfrag_simpleallday", "All day event for small number of days", 1, 1, "Summer Holidays", NULL },
        {"20230901", "resources/calfrag_simpleallday", "All day event for small number of days that finshes before", 0, 1, NULL, NULL },
        {"20270401", "resources/calfrag_recurAlternateWeeks", "Event that occurs alternate weeks - this in the on week", 1, 1, "AltWeeks", NULL},   
        {"20230327", "resources/calfrag_recurAlternateWeeks", "Event that occurs alternate weeks - this in the off week", 0, 1, NULL, NULL},
    };

    uint32_t numfrags = sizeof(frags)/sizeof(frags[0]);

    TEST_ASSERT(numfrags > 0, "numfrags is %d", numfrags);

    for (uint32_t i=0; i < numfrags; i++)
    {
        char *calfragment = test_utils_fileToString(frags[i].path);
        TEST_ASSERT_PTR_NOT_NULL(calfragment);

        setCalendarRange(convertYYYYMMDDtoEpochTime(frags[i].calStartYYYYMMDD), 3);

        char *unparseddataptr = parsePartialDataForEvents(calfragment, &calParsingContext);

        TEST_ASSERT_EQUAL(getRelevantEventCount(), frags[i].relevantEventCount);
        TEST_ASSERT_EQUAL(getTotalEventCount(), frags[i].totalEventCount);

        if(frags[i].event0CheckSummary != NULL)
        {
            TEST_ASSERT_STRINGS_EQUAL(entries[0].name, frags[i].event0CheckSummary);
        }
        if(frags[i].event0CheckLocation != NULL)
        {
            TEST_ASSERT_STRINGS_EQUAL(entries[0].location, frags[i].event0CheckLocation);
        }
    
        free(calfragment);
        resetEntries();
        resetEventStats();
        calParsingContext.calRelevantEvents = 0;
        calParsingContext.calEvents = 0;
    }
    return 0;
}

int main(void)
{
    int rc = 0;

    if(rc == 0)
        rc = testCalFragments();

    return rc;
}