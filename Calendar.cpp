/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
*/

#include "Calendar.h"
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

#include "InkyCalInternal.h"
#include "LogSerial.h"
#include "EventProcessing.h"

//Stuff from secrets.h (that can only be included in one source file as it declares vars)
extern Calendar_t Calendars[];
//end stuff from secrets.h

#define INKYC_ADDTIME_DAYS   0
#define INKYC_ADDTIME_WEEKS  1
#define INKYC_ADDTIME_MONTHS 2
#define INKYC_ADDTIME_YEARS  3

typedef struct recurringEventInfo
{
    time_t initialStart;
    time_t initialEnd;
    time_t repeatUntil;
    uint32_t durationSecs;
    uint32_t recurrenceType; //One of the INKYC_ADDTIME_* constants
    int32_t recurrenceInterval; //count of number of units (unit type defined by recurrenceType)
    int32_t instancesRemaining; //Including currentInstance => -1 means no limit
    //bool daysFilter[7];  //Not yet implement - probably also want a flag to see filtering needed? 
    time_t currentStart;
    time_t currentEnd;
    uint32_t currentStartYYYYMMDDInt;   
    uint32_t currentEndYYYYMMDDInt;  
} recurringEventInfo_t;

uint64_t allEvents = 0;
uint64_t allRelevantEvents = 0;

//set using setCalendarRange()
static time_t CalendarStart; //First day of calendar is day containing this timestamp
static uint32_t DaysRelevent;


//Sets the time period to find events for
// input: calendarStart (epoch time) - indicates the first day 
//                       (doesn't have to be midnight - first day is localtime day containing
//                        that time_t)
// input: numDays - number of days including the first day that events are relevant for
void setCalendarRange(time_t calendar_start, uint32_t numDays)
{
    CalendarStart = calendar_start;
    DaysRelevent = numDays;
}

// Adds days days, weeks, months or years to a time_t in local timezone
// (days may be e.g. 23 hours is timezone changes)
// inputime        - start time for calculation
// offset (input)  - measured in units determined by offsetUnits
// offsetUnits     - One of the INKYC_ADDTIME_* constants
static time_t addTime(time_t inputTime, int32_t offset, uint32_t offsetUnits)
{
    struct tm timeInfo;

    localtime_r(&inputTime, &timeInfo);
    timeInfo.tm_isdst = -1; //calculate based on tz data

    switch(offsetUnits)
    {
        case INKYC_ADDTIME_DAYS:
            timeInfo.tm_mday += offset;
            break;
        
        case INKYC_ADDTIME_WEEKS:
            timeInfo.tm_mday += offset * 7;
            break;
        
        case INKYC_ADDTIME_MONTHS:
            timeInfo.tm_mon += offset;
            break;
        
        case INKYC_ADDTIME_YEARS:
            timeInfo.tm_year += offset;
            break;
        
        default:
            LogSerial_Error("addTime: Invalid unit for adding time: %" PRIu32, offsetUnits);
            logProblem(INKY_SEVERITY_ERROR);
            timeInfo.tm_year += 1000; //Change time to something outside calendar range to help avoid infinite loops
            break;
    }

    return mktime(&timeInfo);
}

//Puts a string representation of a date into
//timestr.
// input: timeinfo
// input: inclYear - true if year should be included in string
// output: timestr: same format as ctime()/asctime() but without time and \n e.g.
//     Wed Jun 30 1993   (with year -15 chars + \0)
//     Wed Jun 30        (w/o year - 10 chars + \0)

void getDateString(char *timeStr, struct tm *pTimeinfo, bool inclYear)
{
    char tempTimeAndDateString[26];
    // Copies time+date string into temporary buffer
    asctime_r(pTimeinfo, tempTimeAndDateString);
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
// input: offset: days into the future from calendar start time
// input: inclYear - true if year should be included in string
// output: timestr: same format as ctime()/asctime() but without time and \n e.g.
//     Wed Jun 30 1993   (with year -16 chars including \0)
//     Wed Jun 30        (w/o year - 11 chars including \0)"

void getDateStringOffsetDays(char* timeStr, int32_t offsetDays, bool inclYear)
{
    // Get seconds since 1.1.1970
    time_t offsetepoch = addTime(CalendarStart, offsetDays, INKYC_ADDTIME_DAYS); 
    
    struct tm offset_tm;
    localtime_r(&offsetepoch, &offset_tm);
    offset_tm.tm_isdst = -1; //if mktime is called on this, figure out is DST is in operation

    getDateString(timeStr, &offset_tm, inclYear);
}

//Returns time now (not calendar start time)
//for e.g. "Updated at " tyupe of uses
//returns 0 on error or number of types (not including \0 added to buffer)
uint32_t  getTimeStringNow(char *buffer, size_t maxlen)
{
    time_t nowSecs = time(nullptr);  
    struct tm now_tm;
    localtime_r(&nowSecs, &now_tm);

    return strftime(buffer, maxlen, "%Y-%m-%d %H:%M", &now_tm);
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
    ltm.tm_isdst = -1; //Figure out whether DST is in operation

    time_t epochtime = mktime(&ltm);

    return epochtime;
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
    ltm.tm_isdst = -1; //Figure out whether DST is in operation

    time_t epochtime = mktime(&ltm);

    return epochtime;
}

int convertEpochTimeToYYYYMMDD(time_t inputTime)
{
    char temp[10];

    struct tm day_tm;
    localtime_r(&inputTime, &day_tm);
    
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
    bool freqValid = false;

    const char *dailyindicator = strstr(recurrenceRule, "FREQ=DAILY");

    if (dailyindicator != NULL)
    {
        toinit->recurrenceType = INKYC_ADDTIME_DAYS;
        freqValid = true;     
    }

    if (!freqValid)
    {
        const char *weeklyindicator = strstr(recurrenceRule, "FREQ=WEEKLY");

        if (weeklyindicator != NULL)
        {
            toinit->recurrenceType = INKYC_ADDTIME_WEEKS;
            freqValid = true;     
        }
    }
    
     if (!freqValid)
    {
        const char *monthlyindicator = strstr(recurrenceRule, "FREQ=MONTHLY");

        if (monthlyindicator != NULL)
        {
            toinit->recurrenceType = INKYC_ADDTIME_MONTHS;
            freqValid = true;     
        }
    }

    if (!freqValid)
    {
        const char *yearlyindicator = strstr(recurrenceRule, "FREQ=YEARLY");

        if (yearlyindicator != NULL)
        {
            toinit->recurrenceType = INKYC_ADDTIME_YEARS;
            freqValid = true;     
        }
    }
   
    if (!freqValid)
    {
        LogSerial_FatalError("Currently can only parse daily/weekly/monthly/yearly recurring event but got recur rule: %s",  recurrenceRule);
        logProblem(INKY_SEVERITY_FATAL);        
        return false; 
    }

    const char *countindicator = strstr(recurrenceRule, "COUNT=");

    if (countindicator != NULL)
    {
        const char *countStrStart = countindicator + strlen("COUNT=");
        const char *countStrEnd = strchr(countStrStart, ';');
        size_t countStrLen = strlen(countStrStart);

        if (countStrEnd != NULL)
        {
            countStrLen = countStrEnd - countStrStart;
        }

        if (countStrLen > 15)
        {
            LogSerial_FatalError("Parse recurring event failed - COUNT= value too large. recur rule: %s",  recurrenceRule);
            logProblem(INKY_SEVERITY_FATAL);
            return false; 
        }
        char countTemp[16];
        memcpy(countTemp, countStrStart, countStrLen);
        countTemp[countStrLen] = '\0';

        long countLong = strtol(countTemp, NULL, 10);

        if(countLong <= 0 || countLong > 1000000L)
        {
            LogSerial_FatalError("Parse recurring event failed - COUNT= value %ld out of range . recur rule: %s",  countLong, recurrenceRule);
            logProblem(INKY_SEVERITY_FATAL);            
            return false; 
        }
        toinit->instancesRemaining = countLong;        
    }
    else
    {
      toinit->instancesRemaining = -1;
    }

    const char *untilindicator = strstr(recurrenceRule, "UNTIL=");

    if (untilindicator != NULL)
    {
        const char *untilStrStart = untilindicator + strlen("UNTIL=");
        const char *untilStrEnd = strchr(untilStrStart, ';');
        size_t untilStrLen = strlen(untilStrStart);

        if (untilStrEnd != NULL)
        {
            untilStrLen = untilStrEnd - untilStrStart;
        }

        if (untilStrLen > 15)
        {
            LogSerial_FatalError("Parse recurring event failed - UNTIL= value too long. recur rule: %s",  recurrenceRule);
            logProblem(INKY_SEVERITY_FATAL);
            return false; 
        }
        char untilTemp[16];
        memcpy(untilTemp, untilStrStart, untilStrLen);
        untilTemp[untilStrLen] = '\0';

        time_t untilEpoch = convertYYYYMMDDtoEpochTime(untilTemp);

        if(untilEpoch <= 0)
        {
            LogSerial_FatalError("Parse recurring event failed - UNTIL= value %" PRIu64 " out of range . recur rule: %s",  untilEpoch, recurrenceRule);
            logProblem(INKY_SEVERITY_FATAL);
            return false; 
        }
        toinit->repeatUntil = untilEpoch;
    }

    if(toinit->repeatUntil == 0 && toinit->instancesRemaining <= 0 )
    {
        LogSerial_FatalError("Currently can only parse recurring event with a fixed COUNT=  or UNTIL= but got recur rule: %s",  recurrenceRule);
        logProblem(INKY_SEVERITY_FATAL);
        return false; 
    }
    
    const char *intervalindicator = strstr(recurrenceRule, "INTERVAL=");

    if (intervalindicator != NULL)
    {
        const char *intervalStrStart = intervalindicator + strlen("INTERVAL=");
        const char *intervalStrEnd = strchr(intervalStrStart, ';');
        size_t intervalStrLen = strlen(intervalStrStart);

        if (intervalStrEnd != NULL)
        {
            intervalStrLen = intervalStrEnd - intervalStrStart;
        }

        if (intervalStrLen > 15)
        {
            LogSerial_FatalError("Parse recurring event failed - INTERVAL= value too large. recur rule: %s",  recurrenceRule);
            logProblem(INKY_SEVERITY_FATAL);
            return false; 
        }
        char intervalTemp[16];
        memcpy(intervalTemp, intervalStrStart, intervalStrLen);
        intervalTemp[intervalStrLen] = '\0';

        long intervalLong = strtol(intervalTemp, NULL, 10);

        if(intervalLong <= 0 || intervalLong > 1000000L)
        {
            LogSerial_FatalError("Parse recurring event failed - INTERVAL= value %ld out of range . recur rule: %s",  intervalLong, recurrenceRule);
            logProblem(INKY_SEVERITY_FATAL);
            return false; 
        }
        toinit->recurrenceInterval = intervalLong;
    }
    else
    {
        LogSerial_Error("Currently expect recur rules to contain INTERVAL= but got recur rule: %s (defaulting to 1)",  recurrenceRule);
        logProblem(INKY_SEVERITY_ERROR);
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

            recurInfo->currentStart = addTime(recurInfo->currentStart, 
                                              recurInfo->recurrenceInterval,
                                              recurInfo->recurrenceType);

            recurInfo->currentEnd   = addTime(recurInfo->currentEnd,
                                              recurInfo->recurrenceInterval,
                                              recurInfo->recurrenceType);

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

    time_t from_epochtime = convertYYYYMMDDTHHMMSSZtoEpochTime(from);

    struct tm from_tm_local;
    localtime_r(&from_epochtime, &from_tm_local);
    from_tm_local.tm_isdst = -1;

    strncpy(dst, asctime(&from_tm_local) + 11, 5);

    dst[5] = '-';

    time_t to_epochtime = convertYYYYMMDDTHHMMSSZtoEpochTime(to);
    
    struct tm to_tm_local;
    localtime_r(&to_epochtime, &to_tm_local);
    to_tm_local.tm_isdst = -1;
    strncpy(dst + 6, asctime(&to_tm_local) + 11, 5);

    dst[11] = 0;

    *timeStamp = from_epochtime;

    char eventDateString[16];
    getDateString(eventDateString, &from_tm_local, true);

    bool matchedDay= false;

    for (int daynum = 0; daynum < DaysRelevent; daynum++)
    {
        char dayDateString[16];
        getDateStringOffsetDays(dayDateString, daynum, true);

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
  
    LogSerial_Verbose2("<<< getFromTo Chosen day %" PRId8, *day);
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
        LogSerial_Error("parseAllDayEvent: Event %s has dates out of range: start %d end %d",
                                pEvents[*pEventIndex].name, dateStartInt, dateEndInt);
        logProblem(INKY_SEVERITY_ERROR);
        return 0;
    }

    int relevantDays = 0;

    time_t nowepoch = CalendarStart;
    struct tm timeinfo;
    
    localtime_r(&nowepoch, &timeinfo);
    timeinfo.tm_isdst = -1; //figure it out based on tz info    

    for (int daynum = 0; daynum < DaysRelevent; daynum++)
    {
        //Get day in the form YYYYMMDD then convert to int: dayInt
        time_t dayunixtime = mktime(&timeinfo);
        int dayInt = convertEpochTimeToYYYYMMDD(dayunixtime);
      
        LogSerial_Verbose2("Comparing day %d to event start %d and end %d\n", dayInt, dateStartInt, dateEndInt);        

        if (dayInt < dateEndInt) //Check it's not in the past (dateEnd is first date after event)
        {
            if (dayInt >= dateStartInt) //Check it's not in the future
            {
                //We need to show this event in column daynum
                LogSerial_Verbose3("parseAllDayEventInstance: Event %s is relevant for day %d : start %d end %d",
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
                    
                    localtime_r(&dayunixtime, &midnight_tm);

                    midnight_tm.tm_sec = 0;
                    midnight_tm.tm_min = 0;
                    midnight_tm.tm_hour = 0;
                    midnight_tm.tm_isdst = -1; //figure out if dst is in force
                    pEvents[(*pEventIndex)].timeStamp = mktime(&midnight_tm);

                    
                    *pEventIndex = *pEventIndex + 1;
                }
                else
                {
                    LogSerial_Error("parseAllDayEventInstance: Event %s (day %d - start %d end %d) - No space in entry list!",
                                    pEvents[*pEventIndex].name, daynum, dateStartInt, dateEndInt);
                    logProblem(INKY_SEVERITY_ERROR);
                }
            }          
        }

        //Now update for the next day
        timeinfo.tm_mday +=1;

        //Convert from day Jan 32nd back to human dates by going to/from epoch time
        time_t dayepochtime;
        dayepochtime = mktime(&timeinfo);
        localtime_r(&dayepochtime, &timeinfo);
        timeinfo.tm_isdst = -1; //dst active is based on tz info
    }

    if (relevantDays > 0)
    {
        LogSerial_Verbose1("parseAllDayEventInstance: Event %s (start %d end %d) - relevant %d days",
                                pEvents[*pEventIndex].name, dateStartInt, dateEndInt, relevantDays);
    }
    else
    {
        LogSerial_Verbose2("parseAllDayEventInstance: Event %s (start %d end %d) - relevant %d days",
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
            logProblem(INKY_SEVERITY_ERROR);
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

    if (relevantDays > 0)
    {
        LogSerial_Info("parseAllDayEvent: Event %s (recurred %" PRIu32 " times based on start %.*s rule %s) - relevant %d days",
                      pEvents[*pEventIndex].name, eventInstances, 8, dateStart, (recurRule != NULL ? recurRule : "unset"), relevantDays);
    }
    else
    {
        LogSerial_Verbose1("parseAllDayEvent: Event %s (recurred %" PRIu32 " times based on start %.*s rule %s) - relevant %d days",
                      pEvents[*pEventIndex].name, eventInstances, 8, dateStart, (recurRule != NULL ? recurRule : "unset"), relevantDays);
    }

    return relevantDays;
}


char *parsePartialDataForEvents(char *rawData, void *context)
{
    CalendarParsingContext_t *calContext = (CalendarParsingContext_t *)context;  
    Calendar_t *pCal = calContext->pCal;

    long scanpos = 0;
    long bytesAvail = strlen(rawData);

    uint64_t batchEvents = 0;
    uint64_t batchEventsRelevant = 0;

    bool eventRelevant = false;
    
    char *unparseddata = rawData;    

    // Search raw data for events
    while (scanpos < bytesAvail)
    { 
        eventRelevant = false;

        // Find next event start and end
        char *startSearch = strstr(rawData + scanpos, "BEGIN:VEVENT");
        char *start;
    
        if (startSearch)
        {
            LogSerial_Verbose1("Found Event Start at %ld", startSearch - rawData);
            start = startSearch + strlen("BEGIN:VEVENT");
        }
        else
        {
            LogSerial_Verbose2("Found no event in last %ld bytes", bytesAvail - scanpos);
            goto mod_exit;          
        }


        char *end = strstr(start, "END:VEVENT");

        if (end)
        {
            LogSerial_Verbose2("Found Event End at pos %d (absolute address %d)", end - rawData, end);
            unparseddata = end;
        }
        else
        {
            LogSerial_Verbose1("Found event without end (position %ld) - won't parse %ld bytes", start-rawData, bytesAvail-(start-rawData));
            LogSerial_Verbose2("Unparsed event fragment:\n%s", start);
            goto mod_exit;
        }

        scanpos = end - rawData;
        size_t eventLength = end - start;

        // Find all relevant event data
        char *summarySearch = (char *)memmem(start, eventLength, "SUMMARY:", strlen("SUMMARY:"));
        char *locationSearch = (char *)memmem(start, eventLength, "LOCATION:", strlen("LOCATION:"));
        char *dtStartSearch = (char *)memmem(start, eventLength, "DTSTART", strlen("DTSTART"));
        char *dtEndSearch = (char *)memmem(start, eventLength, "DTEND", strlen("DTEND"));
        char *recurRuleSearch = (char *)memmem(start, eventLength, "RRULE:", strlen("RRULE:"));

        char *summary = NULL;
        char *location = NULL;
        char *timeStart = NULL;
        char *timeEnd = NULL;
        char *dateStart = NULL;
        char *dateEnd = NULL;
        char *recurRule = NULL;
        char *timeZone = NULL;
        size_t timeZoneLength = 0;

        if (summarySearch)
        {
            summary = summarySearch + strlen("SUMMARY:");
        }
        if (locationSearch)
        {
            location = locationSearch + strlen("LOCATION:");
        }
        if (dtStartSearch)
        {
            char *dtStart = dtStartSearch + strlen("DTSTART");

            if (dtStart[0] == ':')
            {
                //Just a plain time - phew - timeStart is after colon
                timeStart = dtStart + 1;
            }
            else
            {
                char *dtparamsEnd = strchr(dtStart, ':');
                size_t dtparamsLen = dtparamsEnd - dtStart;

                if (dtparamsEnd && dtparamsEnd < end)
                {
                    //Check for TZINFO of the form:
                    //DTSTART;TZID=Europe/London:20181127T193000
                    char *tzidStartSearch = (char *)memmem(dtStart, dtparamsLen,
                                                           "TZID=", strlen("TZID="));

                    if (tzidStartSearch)
                    {
                        timeZone = tzidStartSearch + strlen("TZID=");
                        timeZoneLength = dtparamsEnd - timeZone;
                        timeStart =  dtparamsEnd + 1; //+1 to go past ':' we are pointing at
                    }
                    else
                    {
                        //dates are of the form: DTSTART;VALUE=DATE:
                        char *dateStartSearch = (char *)memmem(dtStart, dtparamsLen,
                                                          "VALUE=DATE", strlen("VALUE=DATE"));
          
                        if (dateStartSearch)
                        {
                            dateStart = dtparamsEnd + 1; //+1 to go past ':' we are pointing at
                        }
                    }
                }
            }
        }

        if (dtEndSearch)
        {
            char *dtEnd = dtEndSearch + strlen("DTEND");

            if (dtEnd[0] == ':')
            {
                //Just a plain time - phew - timeStart is after colon
                timeEnd = dtEnd + 1;
            }
            else
            {
                char *dtparamsEnd = strchr(dtEnd, ':');
                size_t dtparamsLen = dtparamsEnd - dtEnd;

                if (dtparamsEnd && dtparamsEnd < end)
                {
                    //Check for TZINFO of the form:
                    //DTSTART;TZID=Europe/London:20181127T193000
                    char *tzidSearch = (char *)memmem(dtEnd, dtparamsLen,
                                                           "TZID=", strlen("TZID="));

                    if (tzidSearch)
                    {
                        //We don't do anything with a timezone in an end time... 
                        timeEnd =  dtparamsEnd + 1; //+1 to go past ':' we are pointing at
                    }
                    else
                    {
                        //dates are of the form: DTSTART;VALUE=DATE:
                        char *dateEndSearch = (char *)memmem(dtEnd, dtparamsLen,
                                                          "VALUE=DATE", strlen("VALUE=DATE"));
          
                        if (dateEndSearch)
                        {
                            dateEnd = dtparamsEnd + 1; //+1 to go past ':' we are pointing at
                        }
                    }     
                }
            }
        }

        if (recurRuleSearch)
        {
            recurRule = recurRuleSearch + strlen("RRULE:");
        }

        LogSerial_Verbose2("Finished finding fields for event %d (so far: relevant % " PRIu64 ", total %" PRIu64 ")",
                                                 entriesNum, allRelevantEvents, allEvents);

        entry_SetColour(&entries[entriesNum], pCal->eventColour);
        entries[entriesNum].sortTieBreak =  pCal->sortTieBreak;
        
        if (summary)
        {
            LogSerial_Verbose1("Summary: %s", entries[entriesNum].name);
            strncpy(entries[entriesNum].name, summary, strchr(summary, '\n') - summary);
            entries[entriesNum].name[strchr(summary, '\n') - summary] = 0;
        }
        else
        {
            LogSerial_Unusual("Event with no summary: %.*s", eventLength, start);
            entries[entriesNum].name[0] = '\0';
        }

        if (location)
        {
            LogSerial_Verbose1("Location: %s", entries[entriesNum].location);
            strncpy(entries[entriesNum].location, location, strchr(location, '\n') - location);
            entries[entriesNum].location[strchr(location, '\n') - location] = 0;
        }
        else
        {
            entries[entriesNum].location[0] = '\0';
        }

        //TODO: Pass description in
        uint32_t matchresult = INKYR_RESULT_NOOP;
        if (pCal->EventRules)
        {
            matchresult = runEventMatchRules(pCal->EventRules, &entries[entriesNum],  "", recurRule);
        }

        if (matchresult != INKYR_RESULT_DISCARD)
        {
            if (timeStart && timeEnd)
            {
                getToFrom(entries[entriesNum].time, timeStart, timeEnd, &entries[entriesNum].day,
                          &entries[entriesNum].timeStamp);

                LogSerial_Verbose1("Determined day to be: %" PRIu8, entries[entriesNum].day);
            
                if (entries[entriesNum].day >= 0)
                {
                    eventRelevant = true;
                    ++entriesNum;
                }
            }
            else
            {
                if (dateStart && dateEnd)
                {   
                    //Assume date in format YYYYMMDD
                    if (strnlen(dateStart, 8) >= 8 && strnlen(dateEnd, 8) >= 8)
                    {
                        //Null terminate recur rule if there is one
                        char recurRuleBuf[65];
                        if (recurRule)
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
                                logProblem(INKY_SEVERITY_ERROR);
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
                        LogSerial_Unusual("Event with no valid date info - event length %ld", eventLength);
                        LogSerial_Unusual("Event with no valid date info: %.*s", eventLength, start);
                    }
                }
                else
                {
                    LogSerial_Unusual("Event with no valid time info!");
                    LogSerial_Unusual("Event with no valid time info - event length %ld", eventLength);
                    LogSerial_Unusual("Event with no valid time info: %.*s", eventLength, start);
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
    calContext->calEvents += batchEvents;
    calContext->calRelevantEvents += batchEventsRelevant;

    allEvents += batchEvents;
    allRelevantEvents += batchEventsRelevant;

    LogSerial_Info("In this chunk Found %" PRIu64 " relevant events out of %" PRIu64 " (for cal: %" PRIu64 " relevant out of %" PRIu64 ")",
                      batchEventsRelevant, batchEvents,  calContext->calRelevantEvents , calContext->calEvents);
    return unparseddata;
}

//count of events relevant to calendar display
uint64_t getRelevantEventCount()
{
    return allRelevantEvents;
}

//count of all events parsed
uint64_t getTotalEventCount()
{
    return allEvents;
}

void resetEventStats()
{
    allRelevantEvents = 0;
    allEvents = 0;
}