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
#include <stdio.h>

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

//Longest line (including continuation lines) in an ICAL calendar we are prepared to read
#define INKYC_LINEBUF_MAX 500

#define INKYC_LINERC_INCOMPLETE_TOO_LONG 1  //Can't get complete unfolded line, it's too long
#define INKYC_LINERC_INCOMPLETE_SRC      2  //Can't get complete unfolded line - it's incomplete is the src buffer

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

//Hold information about an where in the bufferevernt whilst we parse it
#define INKYC_EVTPARSE_MAXBYTES_TIME 20
#define INKYC_EVTPARSE_MAXBYTES_TIMEZONE  128
#define INKYC_EVTPARSE_MAXBYTES_RECURRULE 128
typedef struct eventParsingDetails
{
    char summary[INKY_ENTRY_MAXBYTES_NAME];
    char location[INKY_ENTRY_MAXBYTES_LOCATION];
    char timeStart[INKYC_EVTPARSE_MAXBYTES_TIME];
    char timeEnd[INKYC_EVTPARSE_MAXBYTES_TIME];
    char dateStart[INKYC_EVTPARSE_MAXBYTES_TIME];
    char dateEnd[INKYC_EVTPARSE_MAXBYTES_TIME];
    char timeZone[INKYC_EVTPARSE_MAXBYTES_TIMEZONE];
    char recurRule[INKYC_EVTPARSE_MAXBYTES_RECURRULE];
    char *foldedDescription;   //We don't unfold description - it can be long and we don't display it
    size_t foldedDescriptionLen;
} eventParsingDetails_t;


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

static int getYYYYMMDDInt4FirstDay()
{
    return convertEpochTimeToYYYYMMDD(CalendarStart);
}

static int getYYYYMMDDInt4LastDay()
{
    struct tm timeinfo;
    localtime_r(&CalendarStart, &timeinfo);
 
    timeinfo.tm_isdst = -1; //dst active is based on tz info
    timeinfo.tm_mday += DaysRelevent;       
    
    time_t lastdayepochtime = mktime(&timeinfo);

    return convertEpochTimeToYYYYMMDD(lastdayepochtime);
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
        LogSerial_Verbose4("recurrence rule did not contain INTERVAL= (got recur rule: %s) therefore defaulting to 1",  recurrenceRule);
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
// output: timestr - time string for event e.g. "14:00-16:00"  (if day is relevant)
// output: day - which calendar day event matches 0 - for first day -1 for not relevant
// output: timestamp - event start in epoch time (if day is relevant)
//
// 
void getTimeString(char *timestr, char *from, char *to, int8_t *day, time_t *timeStamp)
{
    struct tm ltm = {0};
    char temp[128];

    LogSerial_Verbose2(">>> getTimeString (will cpy from addresses %ld and %ld)", from, to);

    //if first day after eventend... can skip
    time_t to_epochtime = convertYYYYMMDDTHHMMSSZtoEpochTime(to);
    int toDateInt = convertEpochTimeToYYYYMMDD(to_epochtime);
    if (getYYYYMMDDInt4FirstDay() > toDateInt)
    {
        LogSerial_Verbose4("Skipping getting timestring as end of event is only %d", toDateInt);
        *day = -1;    // event not in date range we are showing, don't display  
        return;
    }

    //if last day before eventstart....can skip
    time_t from_epochtime = convertYYYYMMDDTHHMMSSZtoEpochTime(from);
    int fromDateInt = convertEpochTimeToYYYYMMDD(from_epochtime);
    if (getYYYYMMDDInt4LastDay() < fromDateInt)
    {
        LogSerial_Verbose4("Skipping getting timestring as start of event is (calendar future): %d", fromDateInt);
        *day = -1;    // event not in date range we are showing, don't display  
        return;
    }


    //If we got here - this looks like a relevant event
    struct tm from_tm_local;
    localtime_r(&from_epochtime, &from_tm_local);
    from_tm_local.tm_isdst = -1;

    strncpy(timestr, asctime(&from_tm_local) + 11, 5);

    timestr[5] = '-';

    
    struct tm to_tm_local;
    localtime_r(&to_epochtime, &to_tm_local);
    to_tm_local.tm_isdst = -1;
    strncpy(timestr + 6, asctime(&to_tm_local) + 11, 5);

    timestr[11] = 0;

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
    LogSerial_Verbose2("<<< getTimeString Chosen day %" PRId8, *day);
}

//On entry to this function 
//events[*pEventIndex] has details like summary, location filled in
//if it's on a matching day we update pEventIndex to refer to the next (as yet unused) event.
//If the event is multiple days long, we will duplicate the event and point pEventIndex
//to after the last used event
//

// returns uint32_t: number of days included in entrylist
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

    //if first day after eventend... can skip
    if (getYYYYMMDDInt4FirstDay() > dateEndInt)
    {
        LogSerial_Verbose4("Skipping instance as end of instamce is only %d", dateEndInt);
        return 0;
    }
    //if last day before eventstart....can skip
    if (getYYYYMMDDInt4LastDay() < dateStartInt)
    {
        LogSerial_Verbose4("Skipping instance as start of instance is (calendar future): %d", dateEndInt);
        return 0;
    }

    int relevantDays = 0;
    struct tm timeinfo;
    
    localtime_r(&CalendarStart, &timeinfo);
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

    if (recurRule != NULL && recurRule[0] != '\0')
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
        
        int lastday = getYYYYMMDDInt4LastDay();
    
        while( getNextOccurence(&recurInfo) )
        {
            if (recurInfo.currentStartYYYYMMDDInt <= lastday)
            {
                eventInstances++;
            
                relevantDays += parseAllDayEventInstance(pEvents, pEventIndex, maxEvents, 
                                                      recurInfo.currentStartYYYYMMDDInt,
                                                      recurInfo.currentEndYYYYMMDDInt);
            }
            else
            {
                //The start of this occurrence is after the end of the calendar
                //we don't need to look at this ooccurrence or any future ones
                LogSerial_Verbose4("Stopping generating occurrences as current start is already %d"
                                  , recurInfo.currentEndYYYYMMDDInt);
                break;
            }
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
                      pEvents[*pEventIndex].name, eventInstances, 8, dateStart, (recurRule != NULL && recurRule[0] != '\0'? recurRule : "unset"), relevantDays);
    }
    else
    {
        LogSerial_Verbose1("parseAllDayEvent: Event %s (recurred %" PRIu32 " times based on start %.*s rule %s) - relevant %d days",
                      pEvents[*pEventIndex].name, eventInstances, 8, dateStart, (recurRule != NULL && recurRule[0] != '\0'? recurRule : "unset"), relevantDays);
    }

    return relevantDays;
}

int32_t getUnfoldedLine(char **ppSrc, char *dest, size_t maxBytes)
{
    char *src = *ppSrc;
    char *destcurpos = dest;
    
    char *destendline = NULL;        // \__ If we find we can't include current line, we'll
    char *srcendline  = NULL;  // /   terminate dest at this point (if set)

    size_t maxNonNullBytes = maxBytes - 1;

    int32_t linerc = 0;
    bool gotCompleteLine = false;

    LogSerial_Verbose5("On entry gUL was given %p\n", src);


    while (*src != '\0' && linerc == 0 && !gotCompleteLine)
    {
        if ((destcurpos - dest) >= maxNonNullBytes)
        {
            linerc = INKYC_LINERC_INCOMPLETE_TOO_LONG;
            break;
        }
        switch(src[0])
        {
            case '\\':
                //See if next is n if so put \n in dest and skip both in src
                if(src[1] == 'n')
                {
                    *destcurpos = '\n';
                    destcurpos++;
                    src +=2;
                }
                else if (src[1] == '\\' || src[1] == ';' || src[1] == ',')
                {
                    //Backslashes seem to escaped to \\ in some ical
                    //; and , seem to be escaped as well
                    *destcurpos = src[1];
                    destcurpos++;
                    src+=2;
                }
                else
                {
                    //doesn't look like an escape sequence - treat as literal backslash
                    *destcurpos = *src;
                    destcurpos++;
                    src++;
                }
                break;

            case '\r':
                //deliberately skip - it's either part of lineending (which we are removing)
                // or a weird character we want to omit from diplayed anyway
                src++;
                break;
            
            case '\n':
                //Line ending
                destendline = destcurpos;
                srcendline = &(src[1]); //if we rewind to this line end - start src at being of next line

                if (src[1] == '\0')
                {
                    //D'oh - can't tell whether it would be a continuation line - don't have next char
                   linerc = INKYC_LINERC_INCOMPLETE_SRC;
                   break;
                }
                else if (src[1] == ' ' || src[1] == '\t')
                {
                    //Continuation line, skip over in src and keep scanning
                    src += 2;
                }
                else
                {
                    //Found the end of current line
                    gotCompleteLine = true;

                    *destcurpos = '\0'; //Ensure output buffer null-terminated
                    *ppSrc = &(src[1]); //Tell caller where next line starts
                    break;
                }
                break;
            
            default:
                //By default copy src -> dest
                *destcurpos = *src;
                destcurpos++;
                src++;
                break;
        }
    }

    if (!gotCompleteLine)
    {
        if (linerc == INKYC_LINERC_INCOMPLETE_TOO_LONG)
        {
            if (destendline != NULL && srcendline != NULL)
            {
                //We'll rewind scan to last \n as a sensible place to leave the scan and return partial line
                *destendline = '\0';
                *ppSrc = srcendline;
            }
            else
            {
               //Line in src too long to ever scan - move source past it and return partial line 

                while(*src != '\0' && *src != '\n')
                {
                    src++;
                }
                if(*src == '\n')
                {
                    *ppSrc =  &(src[1]); //Start scan at begining of next line
                    *destcurpos = '\0'; //Ensure output buffer null-terminated
                }
                else
                {
                    //Can't find a line ending to continue scan from... give up and undo scan entirely
                    linerc = INKYC_LINERC_INCOMPLETE_SRC; 
                    *dest = '\0';  //Set out buffer to be empty
                }
            }
        }
        else
        {
            //Return empty buffer and don't update ppSrc
            *dest = '\0'; 
            linerc = INKYC_LINERC_INCOMPLETE_SRC;
        }
    }

    LogSerial_Verbose5("On exit gUL was sending  %p, %d dest: %s\n", *ppSrc, linerc, dest);

    return linerc;
}

//returns 0 if found all the details for event
//returns INKYC_LINERC_INCOMPLETE_SRC if event details are not complete
int32_t findNextEventDetails(char **ppUnparsedData, eventParsingDetails_t *pEventDetails)
{
    char *currPos = *ppUnparsedData;
    bool inEvent       = false;
    bool foundEventEnd = false;
    int32_t rc = 0;

    while (!foundEventEnd && rc == 0)
    {
        bool movedToNextLineYet = false;

        if(!inEvent)
        {
            if(strncmp(currPos, "BEGIN:VEVENT", strlen("BEGIN:VEVENT")) == 0)
            {
                inEvent = true;
            }
        }
        else
        {
            //Unfold the current line as per rfc5545
            char linebuf[INKYC_LINEBUF_MAX];

            char *srcLineStartPos = currPos;
            int32_t linerc = getUnfoldedLine(&currPos, linebuf, INKYC_LINEBUF_MAX);

            if (linerc == INKYC_LINERC_INCOMPLETE_SRC)
            {
                //giveup the scan
                rc = INKYC_LINERC_INCOMPLETE_SRC;
                break;
            }
            else
            {
                //Unfolding the line will have moved us to the next one
                movedToNextLineYet = true;
            }

            bool usefulField = false;
            switch(linebuf[0])
            {
                case ' ':   //Deliberate fall through to other whitespace case
                case '\t':
                    //If we found the start of a description but not the end yet...
                    if (   pEventDetails->foldedDescription != NULL 
                        && pEventDetails->foldedDescriptionLen == 0)
                    {
                        usefulField = true;

                        //If we finally found the end of the description:
                        if (linerc != INKYC_LINERC_INCOMPLETE_TOO_LONG)
                        {
                            pEventDetails->foldedDescriptionLen = currPos - pEventDetails->foldedDescription;
                        }
                    }
                    break;

                case 'D':
                    //DESCRIPTION or DTSTART or DTEND

                    if (strncmp(linebuf,"DESCRIPTION:", strlen("DESCRIPTION:")) == 0)
                    {
                        usefulField = true;
                        pEventDetails->foldedDescription = srcLineStartPos;
                        
                        if (linerc != INKYC_LINERC_INCOMPLETE_TOO_LONG)
                        {
                            pEventDetails->foldedDescriptionLen = currPos - pEventDetails->foldedDescription;
                        }
                    }
                    else if(strncmp(linebuf,"DTSTART", strlen("DTSTART")) == 0)
                    {
                        usefulField = true;
                        char *dtStart = linebuf + strlen("DTSTART");

                        if (dtStart[0] == ':')
                        {
                            //Just a plain time - phew - timeStart is after colon
                            snprintf(pEventDetails->timeStart, INKYC_EVTPARSE_MAXBYTES_TIME, dtStart + 1);
                        }
                        else
                        {
                            char *dtparamsEnd = strchr(dtStart, ':');
                            size_t dtparamsLen = dtparamsEnd - dtStart;

                            if (dtparamsEnd)
                            {
                                //Check for TZINFO of the form:
                                //DTSTART;TZID=Europe/London:20181127T193000
                                char *tzidStartSearch = (char *)memmem(dtStart, dtparamsLen,
                                                                    "TZID=", strlen("TZID="));

                                if (tzidStartSearch)
                                {
                                    snprintf(pEventDetails->timeZone, INKYC_EVTPARSE_MAXBYTES_TIMEZONE, tzidStartSearch + strlen("TZID="));
                                    snprintf(pEventDetails->timeStart, INKYC_EVTPARSE_MAXBYTES_TIME, dtparamsEnd + 1); //+1 to go past ':' dtParamsEnd points at
                                }
                                else
                                {
                                    //dates are of the form: DTSTART;VALUE=DATE:
                                    char *dateStartSearch = (char *)memmem(dtStart, dtparamsLen,
                                                                    "VALUE=DATE", strlen("VALUE=DATE"));
                    
                                    if (dateStartSearch)
                                    {
                                        snprintf(pEventDetails->dateStart, INKYC_EVTPARSE_MAXBYTES_TIME, dtparamsEnd + 1); //+1 to go past ':' dtParamsEnd points at
                                    }
                                }
                            }
                        }
                    }
                    else if(strncmp(linebuf,"DTEND", strlen("DTEND")) == 0)
                    {
                        usefulField = true;
                        char *dtEnd = linebuf + strlen("DTEND");

                        if (dtEnd[0] == ':')
                        {
                            //Just a plain time - phew - timeEnd is after colon
                            snprintf(pEventDetails->timeEnd, INKYC_EVTPARSE_MAXBYTES_TIME, dtEnd + 1);
                        }
                        else
                        {
                            char *dtparamsEnd = strchr(dtEnd, ':');
                            size_t dtparamsLen = dtparamsEnd - dtEnd;

                            if (dtparamsEnd)
                            {
                                //Check for TZINFO of the form:
                                //DTSTART;TZID=Europe/London:20181127T193000
                                char *tzidSearch = (char *)memmem(dtEnd, dtparamsLen,
                                                                    "TZID=", strlen("TZID="));

                                if (tzidSearch)
                                {
                                    //We don't do anything with a timezone in an end time...
                                    snprintf(pEventDetails->timeEnd, INKYC_EVTPARSE_MAXBYTES_TIME, dtparamsEnd + 1); //+1 to go past ':' dtParamsEnd points at
                                }
                                else
                                {
                                    //dates are of the form: DTSTART;VALUE=DATE:
                                    char *dateEndSearch = (char *)memmem(dtEnd, dtparamsLen,
                                                                    "VALUE=DATE", strlen("VALUE=DATE"));
                    
                                    if (dateEndSearch)
                                    {
                                        snprintf(pEventDetails->dateEnd, INKYC_EVTPARSE_MAXBYTES_TIME, dtparamsEnd + 1); //+1 to go past ':' dtParamsEnd points at
                                    }
                                }
                            }
                        }
                    }
                    break;

                case 'E':
                    //END:VEVENT
                    if (strncmp(linebuf,"END:VEVENT", strlen("END:VEVENT")) == 0)
                    {
                        usefulField = true;
                        foundEventEnd = true; 
                        break;
                    }
                    break;
                
                case 'L':
                    //LOCATION
                    if (strncmp(linebuf,"LOCATION:", strlen("LOCATION:")) == 0)
                    {
                        usefulField = true;
                        snprintf(pEventDetails->location, INKY_ENTRY_MAXBYTES_LOCATION,
                                 "%s",linebuf+strlen("LOCATION:"));
                    }
                    break;
                
                case 'R':
                    //RRULE
                    if (strncmp(linebuf,"RRULE:", strlen("RRULE:")) == 0)
                    {
                        usefulField = true;
                        snprintf(pEventDetails->recurRule, INKYC_EVTPARSE_MAXBYTES_RECURRULE,
                                 "%s",linebuf+strlen("RRULE:"));
                    }
                
                case 'S':
                    //SUMMARY
                    if (strncmp(linebuf,"SUMMARY:", strlen("SUMMARY:")) == 0)
                    {
                        usefulField = true;
                        snprintf(pEventDetails->summary, INKY_ENTRY_MAXBYTES_NAME,
                                 "%s",linebuf+strlen("SUMMARY:"));
                    }
                    break;
                
                default:
                    //Lots of fields we don't use
                    break;

            }

            if (!usefulField)
            {
                LogSerial_Verbose4("SkippingDuringParse: %s", linebuf);
            }
        }

        //Move to next line - if we haven't already
        if (!movedToNextLineYet)
        {
            currPos = strchr(currPos, '\n');

            if (!currPos)
            {
                //giveup the scan - haven't got a complete line
                rc = INKYC_LINERC_INCOMPLETE_SRC;
            }
            else
            {
               currPos++; //move past '\n'
            }
        }
    }

    //If we found a description but not the end of the dscription we are not complete
    if (rc == 0)
    {
       if((pEventDetails->foldedDescriptionLen == 0) != (pEventDetails->foldedDescription == NULL))
       {
           //We must have a bug as we shouldn't have found event end etc without first see description end!
           LogSerial_FatalError("We thought we had finished parse of event but hadn't found descripton end: %s",  (pEventDetails->summary ? pEventDetails->summary : "No summary"));
           logProblem(INKY_SEVERITY_FATAL);        
           rc = INKYC_LINERC_INCOMPLETE_SRC;
       }
    }

    if (rc == 0)
    {
        *ppUnparsedData = currPos;
    }
    return rc;
}

char *parsePartialDataForEvents(char *rawData, void *context)
{
    CalendarParsingContext_t *calContext = (CalendarParsingContext_t *)context;  
    Calendar_t *pCal = calContext->pCal;


    uint64_t batchEvents = 0;
    uint64_t batchEventsRelevant = 0;
    char *unparseddata = rawData; 
    
    int evtrc = 0;
    
    // Search raw data for events
    while (evtrc == 0)
    {
        bool eventRelevant = false;
        eventParsingDetails_t eventDetails = {0};

        evtrc = findNextEventDetails(&unparseddata, &eventDetails);

        if (evtrc == 0)
        {
            LogSerial_Verbose2("Finished finding fields for event %d (so far: relevant % " PRIu64 ", total %" PRIu64 ")",
                                                 entriesNum, allRelevantEvents, allEvents);

            entry_SetColour(&entries[entriesNum], pCal->eventColour);
            entries[entriesNum].sortTieBreak =  pCal->sortTieBreak;
        
            if (eventDetails.summary[0] != '\0')
            {
                LogSerial_Verbose1("Summary: %s", eventDetails.summary);
                strcpy(entries[entriesNum].name, eventDetails.summary);
            }
            else
            {
                LogSerial_Unusual("Event with no summary. Description: %.*s", eventDetails.foldedDescriptionLen,  eventDetails.foldedDescription);
                entries[entriesNum].name[0] = '\0';
            }

            if (eventDetails.location[0] != '\0')
            {
                LogSerial_Verbose1("Location: %s", eventDetails.location);
                strcpy(entries[entriesNum].location, eventDetails.location);
            }
            else
            {
                entries[entriesNum].location[0] = '\0';
            }

            uint32_t matchresult = INKYR_RESULT_NOOP;
            if (pCal->EventRules)
            {
                matchresult = runEventMatchRules(pCal->EventRules, &entries[entriesNum],  
                                                 eventDetails.foldedDescription, eventDetails.foldedDescriptionLen,
                                                 eventDetails.recurRule);
            }

            if (matchresult != INKYR_RESULT_DISCARD)
            {
                if (eventDetails.timeStart[0] != '\0' && eventDetails.timeEnd[0] != '\0')
                {
                    getTimeString(entries[entriesNum].time,
                                  eventDetails.timeStart, eventDetails.timeEnd, 
                                  &entries[entriesNum].day, &entries[entriesNum].timeStamp);

                    LogSerial_Verbose1("Determined day to be: %" PRId8, entries[entriesNum].day);
            
                    if (entries[entriesNum].day >= 0)
                    {
                        eventRelevant = true;
                        ++entriesNum;
                    }
                }
                else
                {
                    if (eventDetails.dateStart[0] != '\0' && eventDetails.dateEnd[0] != '\0')
                    {   
                        //Assume date in format YYYYMMDD
                        if (strnlen(eventDetails.dateStart, 8) >= 8 && strnlen(eventDetails.dateEnd, 8) >= 8)
                        {
                            if(parseAllDayEvent(entries, &entriesNum, MAX_ENTRIES, 
                                                eventDetails.dateStart, eventDetails.dateEnd, 
                                                eventDetails.recurRule) > 0)
                            {
                                eventRelevant = true;
                            }
                        }
                        else
                        {
                            LogSerial_Unusual("Event with no valid date info! Event Summary: %s TimeStart %s TimeEnd %s DateStart: %s DateEnd %s",
                                                  eventDetails.summary, eventDetails.timeStart, eventDetails.timeEnd, eventDetails.dateStart, eventDetails.dateEnd);
                              
                        }
                    }
                    else
                    {
                            LogSerial_Unusual("Event with no valid date info! Event Summary: %s TimeStart %s TimeEnd %s DateStart: %s DateEnd %s",
                                                  eventDetails.summary, eventDetails.timeStart, eventDetails.timeEnd, eventDetails.dateStart, eventDetails.dateEnd);
                    }
                }
            }

            if (eventRelevant)
            {
                ++batchEventsRelevant;
            }
            ++batchEvents;
        }
    }
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
