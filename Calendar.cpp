#include "Calendar.h"
#include <string.h>

#include "LogSerial.h"
#include "EventProcessing.h"

//Stuff from secrets.h (that can only be included in one source file as it declares vars)
extern Calendar_t Calendars[];
//end stuff from secrets.h

typedef struct recurringEventInfo
{
    time_t initialStart;
    time_t initialEnd;
    time_t repeatUntil;
    uint32_t durationSecs;
    uint32_t recurrenceType;
    uint32_t recurrenceInterval; //count of number of units (unit type defined by recurrenceType)
    int32_t instancesRemaining; //Including currentInstance => -1 means no limit
    //bool daysFilter[7];  //Not yet implement - probably also want a flag to see filtering needed? 
    time_t currentStart;
    time_t currentEnd;
    uint32_t currentStartYYYYMMDDInt;   
    uint32_t currentEndYYYYMMDDInt;  
} recurringEventInfo_t;

#define INKY_RECUR_TYPE_WEEKLY  1


//Puts a string representation of a date into
//timestr.
// input: unixtime: seconds since 1.1.1970
// input: inclYear - true if year should be included in string
// output: timestr: same format as ctime()/asctime() but without time and \n e.g.
//     Wed Jun 30 1993   (with year -15 chars + \0)
//     Wed Jun 30        (w/o year - 10 chars + \0)

void getDateString(char *timeStr, time_t unixtime, bool inclYear)
{
    //Convert time in unixtime (seconds since 1.1.1970) to a tm struct
    struct tm timeinfo;
    gmtime_r(&unixtime, &timeinfo);

    char tempTimeAndDateString[26];
    // Copies time+date string into temporary buffer
    strcpy(tempTimeAndDateString, asctime(&timeinfo));

    if (inclYear)
    {
        //Move the year+\n\0 on top of the time in the string from asctime()
        memmove(tempTimeAndDateString+11, tempTimeAndDateString+20, 6);
        tempTimeAndDateString[15] = '\0';        
    }
    else
    {
        tempTimeAndDateString[10] = '\0';
    }

    strcpy(timeStr, tempTimeAndDateString);
}

//Puts a string representation of a date into
//timestr.
// input: offset: seconds into the future from now
// input: inclYear - true if year should be included in string
// output: timestr: same format as ctime()/asctime() but without time and \n e.g.
//     Wed Jun 30 1993   (with year -16 chars including \0)
//     Wed Jun 30        (w/o year - 11 chars including \0)"

void getDateStringOffset(char* timeStr, long offSet, bool inclYear)
{
    // Get seconds since 1.1.1970
    //TODO: Consider timezone handling
    time_t nowSecs = time(nullptr) + (long)timeZone * 3600L + offSet;

    getDateString(timeStr, nowSecs, inclYear);
}

//used in getToFrom
time_t convertYYYYMMDDTHHMMSSZtoEpochTime(const char *strYYYYMMDDTHHMMSSZ)
{
    struct tm ltm = {0};
    char temp[40];

    //Creating in "temp" (a version of time in a form strptime accepts)
    strncpy(temp, strYYYYMMDDTHHMMSSZ, 16);
    temp[16] = 0;
    LogSerial_Verbose5("Parsing from: %s", temp);

    // https://github.com/esp8266/Arduino/issues/5141, quickfix
    memmove(temp + 5, temp + 4, 16);
    memmove(temp + 8, temp + 7, 16);
    memmove(temp + 14, temp + 13, 16);
    memmove(temp + 16, temp + 15, 16);
    temp[4] = temp[7] = temp[13] = temp[16] = '-';

    strptime(temp, "%Y-%m-%dT%H-%M-%SZ", &ltm);

    //TODO: consider suspect timezone handling
    time_t epochtime_local = mktime(&ltm) + (time_t)timeZone * 3600L;

    return epochtime_local;
}

//Used in initialiseRecurringAllDayEvent
time_t convertYYYYMMDDtoEpochTime(const char *dayYYYYMMDD)
{
    struct tm ltm = {0};
    char temp[40];

    //Creating in "temp" (a version of time in a form strptime accepts)
    strncpy(temp, dayYYYYMMDD, 9);
    temp[8] = 0;
    LogSerial_Verbose5("Parsing from: %s", temp);

    // https://github.com/esp8266/Arduino/issues/5141, quickfix
    memmove(temp + 5, temp + 4, 16);
    memmove(temp + 8, temp + 7, 16);
    temp[4] = temp[7] = '-';

    strptime(temp, "%Y-%m-%dT", &ltm);

    //TODO: consider suspect timezone handling
    time_t epochtime_local = mktime(&ltm) + (time_t)timeZone * 3600L;

    return epochtime_local;
}

int convertEpochTimeToYYYYMMDD(time_t inputTime)
{
    char temp[10];

    struct tm day_tm;
    gmtime_r(&inputTime, &day_tm);
    strftime(temp, 9, "%Y%m%d", &day_tm);
    long day = strtol(temp, NULL, 10);

    return (int)day;
}

bool initialiseRecurringAllDayEvent(recurringEventInfo_t *toinit,
                                    const char *recurrenceRule,
                                    const char *dayStartYYYYMMDD,
                                    const char *dayEndYYYYMMDD)
{
    memset(toinit, 0, sizeof(recurringEventInfo_t));

    toinit->initialStart = convertYYYYMMDDtoEpochTime(dayStartYYYYMMDD);
    toinit->initialEnd   = convertYYYYMMDDtoEpochTime(dayEndYYYYMMDD);

    //Parse recurrence rule. Example rule:
    //RRULE:FREQ=WEEKLY;WKST=MO;COUNT=26;INTERVAL=2;BYDAY=FR

    char *weeklyindicator = strstr(recurrenceRule, "FREQ=WEEKLY");

    if (weeklyindicator != NULL)
    {
        toinit->recurrenceType = INKY_RECUR_TYPE_WEEKLY;      
    }
    else
    {
        LogSerial_FatalError("Currently can only parse weekly recurring event but got recur rule: %s",  recurrenceRule);
        return false; 
    }

    char *countindicator = strstr(recurrenceRule, "COUNT=");

    if (countindicator != NULL)
    {
        char *countStrStart = countindicator + strlen("COUNT=");
        char *countStrEnd = strchr(countStrStart, ';');
        size_t countStrLen = strlen(countStrStart);

        if (countStrEnd != NULL)
        {
            countStrLen = countStrEnd - countStrStart;
        }

        if (countStrLen > 15)
        {
            LogSerial_FatalError("Parse recurring event failed - COUNT= value too large. recur rule: %s",  recurrenceRule);
            return false; 
        }
        char countTemp[16];
        memcpy(countTemp, countStrStart, countStrLen);
        countTemp[countStrLen] = '\0';

        long countLong = strtol(countTemp, NULL, 10);

        if(countLong <= 0 || countLong > 1000000L)
        {
            LogSerial_FatalError("Parse recurring event failed - COUNT= value %ld out of range . recur rule: %s",  countLong, recurrenceRule);
            return false; 
        }
        toinit->instancesRemaining = countLong;        
    }
    else
    {
      toinit->instancesRemaining = -1;
    }

    char *untilindicator = strstr(recurrenceRule, "UNTIL=");

    if (untilindicator != NULL)
    {
        char *untilStrStart = untilindicator + strlen("UNTIL=");
        char *untilStrEnd = strchr(untilStrStart, ';');
        size_t untilStrLen = strlen(untilStrStart);

        if (untilStrEnd != NULL)
        {
            untilStrLen = untilStrEnd - untilStrStart;
        }

        if (untilStrLen > 15)
        {
            LogSerial_FatalError("Parse recurring event failed - UNTIL= value too long. recur rule: %s",  recurrenceRule);
            return false; 
        }
        char untilTemp[16];
        memcpy(untilTemp, untilStrStart, untilStrLen);
        untilTemp[untilStrLen] = '\0';

        time_t untilEpoch = convertYYYYMMDDtoEpochTime(untilTemp);

        if(untilEpoch <= 0)
        {
            LogSerial_FatalError("Parse recurring event failed - UNTIL= value %ld out of range . recur rule: %s",  untilEpoch, recurrenceRule);
            return false; 
        }
        toinit->repeatUntil = untilEpoch;
    }

    if(toinit->repeatUntil == 0 && toinit->instancesRemaining <= 0 )
    {
        LogSerial_FatalError("Currently can only parse recurring event with a fixed COUNT=  or UNTIL= but got recur rule: %s",  recurrenceRule);
        return false; 
    }
    
    char *intervalindicator = strstr(recurrenceRule, "INTERVAL=");

    if (intervalindicator != NULL)
    {
        char *intervalStrStart = intervalindicator + strlen("INTERVAL=");
        char *intervalStrEnd = strchr(intervalStrStart, ';');
        size_t intervalStrLen = strlen(intervalStrStart);

        if (intervalStrEnd != NULL)
        {
            intervalStrLen = intervalStrEnd - intervalStrStart;
        }

        if (intervalStrLen > 15)
        {
            LogSerial_FatalError("Parse recurring event failed - INTERVAL= value too large. recur rule: %s",  recurrenceRule);
            return false; 
        }
        char intervalTemp[16];
        memcpy(intervalTemp, intervalStrStart, intervalStrLen);
        intervalTemp[intervalStrLen] = '\0';

        long intervalLong = strtol(intervalTemp, NULL, 10);

        if(intervalLong <= 0 || intervalLong > 1000000L)
        {
            LogSerial_FatalError("Parse recurring event failed - INTERVAL= value %ld out of range . recur rule: %s",  intervalLong, recurrenceRule);
            return false; 
        }
        toinit->recurrenceInterval = intervalLong;
    }
    else
    {
        LogSerial_Error("Currently expect recur rules to contain INTERVAL= but got recur rule: %s (defaulting to 1)",  recurrenceRule);
        toinit->recurrenceInterval = 1;
    }

    return true;    
}

bool getNextOccurence(recurringEventInfo_t *recurInfo)
{
    bool foundOccurrence = false;

    if (recurInfo->currentStartYYYYMMDDInt == 0)
    {
        //First event
        recurInfo->currentStart = recurInfo->initialStart;
        recurInfo->currentEnd   = recurInfo->initialEnd;
        foundOccurrence = true;
    }
    else
    {
        if (recurInfo->instancesRemaining != 0)
        {
            time_t timedelta = 0;

            if (recurInfo->recurrenceType == INKY_RECUR_TYPE_WEEKLY)
            {
                timedelta = 7*24*60*60 * recurInfo->recurrenceInterval;
            }
            recurInfo->currentStart += timedelta;
            recurInfo->currentEnd +=  timedelta;

            if (recurInfo->instancesRemaining > 0)
            {
                recurInfo->instancesRemaining --;
            }

            if (    recurInfo->repeatUntil == 0 
                 || recurInfo->currentStart < recurInfo->repeatUntil)
            {
                foundOccurrence = true;
            }
        }
    }

    if (foundOccurrence)
    {
        recurInfo->currentStartYYYYMMDDInt = convertEpochTimeToYYYYMMDD(recurInfo->currentStart);
        recurInfo->currentEndYYYYMMDDInt = convertEpochTimeToYYYYMMDD(recurInfo->currentEnd);
    }

    return foundOccurrence;
}

// Format event times - converting to timezone offset
// input: from - start time for event in YYYYMMDDTHHMMSSZ e.g. 19970901T130000Z
// input: to   - end time for event in YYYYMMDDTHHMMSSZ e.g. 19970901T130000Z
// output: dst - date string for event e.g. "14:00-16:00"
// output: day - which calendar day event matches 0 - for first day
// output: timestamp - event start in epoch time
// 
void getToFrom(char *dst, char *from, char *to, int8_t *day, time_t *timeStamp)
{
    struct tm ltm = {0};
    char temp[128];

    LogSerial_Verbose2(">>> getToFrom (will cpy from addresses %ld and %ld)", from, to);

    time_t from_epochtime_local = convertYYYYMMDDTHHMMSSZtoEpochTime(from);

    struct tm from_tm_local;
    gmtime_r(&from_epochtime_local, &from_tm_local);
    strncpy(dst, asctime(&from_tm_local) + 11, 5);

    dst[5] = '-';

    time_t to_epochtime_local = convertYYYYMMDDTHHMMSSZtoEpochTime(to);
    
    struct tm to_tm_local;
    gmtime_r(&to_epochtime_local, &to_tm_local);
    strncpy(dst + 6, asctime(&to_tm_local) + 11, 5);

    dst[11] = 0;

    *timeStamp = from_epochtime_local;

    char eventDateString[16];
    getDateString(eventDateString, from_epochtime_local, true);

    bool matchedDay= false;

    for (int daynum = 0; daynum < DAYS_SHOWN; daynum++)
    {
        char dayDateString[16];
        getDateStringOffset(dayDateString, daynum*24*3600L, true);

        if (strcmp(eventDateString, dayDateString) == 0)
        {
            matchedDay = true;
            *day = daynum;
        }
    }

    if (!matchedDay)
    {
        *day = -1;    // event not in date range we are showing, don't display  
    }
  
    LogSerial_Verbose2("<<< getFromTo Chosen day %d", *day);
}

//On entry to this function 
//events[*pEventIndex] has details like summary, location filled in
//if it's on a matching day we update pEventIndex to refer to the next (as yet unused) event.
//If the event is multiple days long, we will duplicate the event and point pEventIndex
//to after the last used event
//
// dateStart and dateEnd are both expected to point to array of 8 chars in the form YYYYMMDD
//
// uint32_t: number of days included in entrylist
uint32_t parseAllDayEventInstance(entry_t *pEvents, int *pEventIndex, int maxEvents, int dateStartInt, int dateEndInt)
{
    if (    dateStartInt < 20000101 || dateStartInt > 22000101 
         || dateEndInt   < 20000101 || dateEndInt   > 22000101  )
    {
        LogSerial_Error("parseAllDayEvent: Event %s has dates out of range: start %ld end %ld",
                                pEvents[*pEventIndex].name, dateStartInt, dateEndInt);
        return 0;
    }

    int relevantDays = 0;

    for (int daynum = 0; daynum < DAYS_SHOWN; daynum++)
    {
        //Get day in the form YYYYMMDD then convert to int: dayInt
        //TODO: Consider timezone handling
        time_t dayunixtime = time(nullptr) + (long)timeZone * 3600L +  daynum * 24 * 3600L;
        int dayInt = convertEpochTimeToYYYYMMDD(dayunixtime);
      
        LogSerial_Verbose2("Comparing day %d to event start %ld and end %ld\n", dayInt, dateStartInt, dateEndInt);        

        if (dayInt < dateEndInt) //Check it's not in the past (dateEnd is first date after event)
        {
            if (dayInt >= dateStartInt) //Check it's not in the future
            {
                //We need to show this event in column daynum
                LogSerial_Verbose3("parseAllDayEventInstance: Event %s is relevant for day %ld : start %ld end %ld",
                                  pEvents[*pEventIndex].name, daynum, dateStartInt, dateEndInt);
                relevantDays++;
               
                if (*pEventIndex < maxEvents - 1)
                {
                    //Copy the partial event we are completing to the next slot (in case we need it for the next day)
                    //and complete the slot that we copied
                    memcpy(&pEvents[(*pEventIndex) + 1], &pEvents[*pEventIndex], sizeof(entry_t));
                    pEvents[(*pEventIndex)].day = daynum;
                    strcpy(pEvents[(*pEventIndex)].time, "");

                    //Work out timestamp for midnight at start of this day
                    struct tm midnight_tm;
                    
                    gmtime_r(&dayunixtime, &midnight_tm);

                    midnight_tm.tm_sec = 0;
                    midnight_tm.tm_min = 0;
                    midnight_tm.tm_hour = timeZone;
                    pEvents[(*pEventIndex)].timeStamp = mktime(&midnight_tm);

                    
                    *pEventIndex = *pEventIndex + 1;
                }
                else
                {
                    LogSerial_Error("parseAllDayEventInstance: Event %s (day %d - start %ld end %ld) - No space in entry list!",
                                    pEvents[*pEventIndex].name, daynum, dateStartInt, dateEndInt);
                }
            }          
        }
    }

    if (relevantDays > 0)
    {
        LogSerial_Info("parseAllDayEventInstance: Event %s (start %ld end %ld) - relevant %d days",
                                pEvents[*pEventIndex].name, dateStartInt, dateEndInt, relevantDays);
    }
    else
    {
        LogSerial_Verbose2("parseAllDayEventInstance: Event %s (start %ld end %ld) - relevant %d days",
                                pEvents[*pEventIndex].name, dateStartInt, dateEndInt, relevantDays);
    }

    return relevantDays;
}

uint32_t parseAllDayEvent(entry_t *pEvents, int *pEventIndex, int maxEvents, char *dateStart, char *dateEnd, char *recurRule)
{   
    uint32_t relevantDays = 0;
    uint32_t eventInstances = 0;

    if (recurRule != NULL)
    {
        recurringEventInfo_t recurInfo;

        if(!initialiseRecurringAllDayEvent(&recurInfo,
                                           recurRule,
                                           dateStart,
                                           dateEnd))
        {
            LogSerial_Error("Failed to parse event with recur rule: %s", recurRule);
            return 0;
        }

        while( getNextOccurence(&recurInfo) )
        {
            eventInstances++;

            relevantDays += parseAllDayEventInstance(pEvents, pEventIndex, maxEvents, 
                                              recurInfo.currentStartYYYYMMDDInt,
                                              recurInfo.currentEndYYYYMMDDInt);
        }
    }
    else
    {
        //convert dateStart/dateEnd to ints 
        //We could compare as strings - but this is easier to reason about and not performance sensitive
        char temp[9];
        strncpy(temp, dateStart, 8);
        temp[8] = '\0';
        int dateStartInt = (int)strtol(temp, NULL, 10);

        strncpy(temp, dateEnd, 8);
        temp[8] = '\0';    
        int dateEndInt   = (int)strtol(dateEnd, NULL, 10);

        eventInstances++;
        relevantDays = parseAllDayEventInstance(pEvents, pEventIndex, maxEvents, dateStartInt, dateEndInt);
    }

    LogSerial_Info("parseAllDayEvent: Event %s (recurred %u times based on start %.*s rule %s) - relevant %d days",
                      pEvents[*pEventIndex].name, eventInstances, 8, dateStart, (recurRule != NULL ? recurRule : "unset"), relevantDays);

    return relevantDays;
}


char *parsePartialDataForEvents(char *rawData, void *context)
{
    CalendarParsingContext_t *calContext = (CalendarParsingContext_t *)context;  
    Calendar_t *pCal = calContext->pCal;

    long i = 0;
    long n = strlen(rawData);

    uint64_t batchEvents = 0;
    uint64_t batchEventsRelevant = 0;

    bool eventRelevant = false;
    
    char *unparseddata = rawData;    

    // Search raw data for events
    while (i < n && strstr(rawData + i, "BEGIN:VEVENT"))
    { 
        eventRelevant = false;

        // Find next event start and end
        i = strstr(rawData + i, "BEGIN:VEVENT") - rawData + 12;
        LogSerial_Verbose1("Found Event Start at %d", i);

        char *end = strstr(rawData + i, "END:VEVENT");

        if (end)
        {
            LogSerial_Verbose2("Found Event End at pos %d (absolute address %d)", end - rawData, end);
            unparseddata = end;
        }
        else
        {
            LogSerial_Info("Found event without end (position %ld) - won't parse %ld bytes", i, n-i);
            LogSerial_Verbose1("Unparsed event fragment:\n%s", rawData + i);
            goto mod_exit;
        }

        // Find all relevant event data
        char *summarySearch = strstr(rawData + i, "SUMMARY:");
        char *locationSearch = strstr(rawData + i, "LOCATION:");
        char *timeStartSearch = strstr(rawData + i, "DTSTART:");
        char *timeEndSearch = strstr(rawData + i, "DTEND:");
        char *dateStartSearch = strstr(rawData + i, "DTSTART;VALUE=DATE:");
        char *dateEndSearch = strstr(rawData + i, "DTEND;VALUE=DATE:");
        char *recurRuleSearch = strstr(rawData + i, "RRULE:");

        char *summary = NULL;
        char *location = NULL;
        char *timeStart = NULL;
        char *timeEnd = NULL;
        char *dateStart = NULL;
        char *dateEnd = NULL;
        char *recurRule = NULL;

        if (summarySearch)
        {
            summary = summarySearch + strlen("SUMMARY:");
        }
        if (locationSearch)
        {
            location = locationSearch + strlen("LOCATION:");
        }
        if (timeStartSearch)
        {
            timeStart = timeStartSearch + strlen("DTSTART:");
        }
        if (timeEndSearch)
        {
            timeEnd = timeEndSearch + strlen("DTEND:");
        }
        if (dateStartSearch)
        {
            dateStart = dateStartSearch + strlen("DTSTART;VALUE=DATE:");
        }
        if (dateEndSearch)
        {
            dateEnd = dateEndSearch + strlen("DTEND;VALUE=DATE:");
        }
        if (recurRuleSearch)
        {
            recurRule = recurRuleSearch + strlen("RRULE:");
        }

        LogSerial_Verbose2("Finished finding fields for event %d (so far: relevant %d, total %d)",
                                                 entriesNum, foundEventsRelevant, foundEventsTotal);

        entry_SetColour(&entries[entriesNum], pCal->eventColour);

        if (summary && summary < end)
        {
            strncpy(entries[entriesNum].name, summary, strchr(summary, '\n') - summary);
            entries[entriesNum].name[strchr(summary, '\n') - summary] = 0;
            LogSerial_Verbose1("Summary: %s", entries[entriesNum].name);
        }
        else
        {
            LogSerial_Unusual("Event with no summary: %.*s", end - rawData - i, rawData + i);
            entries[entriesNum].name[0] = '\0';
        }

        if (location && location < end)
        {
            strncpy(entries[entriesNum].location, location, strchr(location, '\n') - location);
            entries[entriesNum].location[strchr(location, '\n') - location] = 0;
            LogSerial_Verbose1("Location: %s", entries[entriesNum].location);
        }
        else
        {
            entries[entriesNum].location[0] = '\0';
        }

        //TODO: Pass description in
        uint32_t matchresult = runEventMatchRules(pCal->EventRules, &entries[entriesNum],  "", recurRule);

        if (matchresult != INKYR_RESULT_DISCARD)
        {
            if (timeStart && timeEnd && timeStart < end && timeEnd < end)
            {
                getToFrom(entries[entriesNum].time, timeStart, timeEnd, &entries[entriesNum].day,
                          &entries[entriesNum].timeStamp);

                LogSerial_Verbose1("Determined day to be: %d", entries[entriesNum].day);
            
                if (entries[entriesNum].day >= 0)
                {
                    eventRelevant = true;
                    ++entriesNum;
                }
            }
            else
            {
                if (dateStart && dateEnd && dateStart < end && dateEnd < end)
                {   
                    //Assume date in format YYYYMMDD
                    if (strnlen(dateStart, 8) >= 8 && strnlen(dateEnd, 8) >= 8)
                    {
                        //Null terminate recur rule if there is one
                        char recurRuleBuf[65];
                        if (recurRule && recurRule < end)
                        {
                            size_t rulelen = strchr(recurRule, '\n') - recurRule;

                            if (rulelen <= 64 && rulelen > 0)
                            {
                                strncpy(recurRuleBuf, recurRule, rulelen);
                                recurRuleBuf[rulelen] = '\0';
                                recurRule = recurRuleBuf;
                            }
                            else
                            {
                                LogSerial_Error("Recur Rule - too long: %u", rulelen);
                                recurRule = NULL;
                            }
                        }
                        else
                        {
                            recurRule = NULL;
                        }

                        if(parseAllDayEvent(entries, &entriesNum, MAX_ENTRIES, dateStart, dateEnd, recurRule) > 0)
                        {
                            eventRelevant = true;
                        }
                    }
                    else
                    {
                        LogSerial_Unusual("Event with no valid date info!");
                        LogSerial_Unusual("Event with no valid date info - event length %ld", end - rawData - i);
                        LogSerial_Unusual("Event with no valid date info: %.*s", end - rawData - i, rawData + i);
                    }
                }
                else
                {
                    LogSerial_Unusual("Event with no valid time info!");
                    LogSerial_Unusual("Event with no valid time info - event length %ld", end - rawData - i);
                    LogSerial_Unusual("Event with no valid time info: %.*s", end - rawData - i, rawData + i);
                }
            }

            if (eventRelevant)
            {
              ++batchEventsRelevant;
            }
            ++batchEvents;
        }
    }
mod_exit:
    calContext->totalEvents += batchEvents;
    calContext->totalEventsRelevant += batchEventsRelevant;

    LogSerial_Info("In this chunk Found %" PRIu64 " relevant events out of %" PRIu64 " (in total %" PRIu64 " relevant out of %" PRIu64 ")",
                      batchEventsRelevant, batchEvents,  calContext->totalEventsRelevant , calContext->totalEvents);
    return unparseddata;
}
