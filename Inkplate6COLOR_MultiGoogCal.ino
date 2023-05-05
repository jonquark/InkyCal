/*
   Shows selected events from multiple calendars
   Based on the Inkplate6COLOR_Google_Calendar example for Soldered Inkplate 6COLOR
   
   
   For this code you will need only USB cable and Inkplate 6COLOR.
   Select "Soldered Inkplate 6COLOR" from Tools -> Board menu.
   Don't have "Soldered Inkplate 6COLOR" option? Follow our tutorial and add it:
   https://soldered.com/learn/add-inkplate-6-board-definition-to-arduino-ide/

   For this to work you need to change your timezone, wifi credentials and your private calendar url
   which you can find following these steps:

    1. Open your google calendar
    2. Click the 3 menu dots of the calendar you want to access at the bottom of left hand side
    3. Click 'Settings and sharing'
    4. Navigate to 'Integrate Calendar'
    5. Take the 'Secret address in iCal format'

   (https://support.google.com/calendar/thread/2408874?hl=en)

   Want to learn more about Inkplate? Visit www.inkplate.io
   Looking to get support? Write on our forums: https://forum.soldered.com/
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#ifndef ARDUINO_INKPLATECOLOR
#error "Wrong board selection for this example, please select Soldered Inkplate 6COLOR in the boards menu."
#endif

// Include Inkplate library to the sketch
#include "Inkplate.h"

// Including fonts
#include "Fonts/FreeSans12pt7b.h"
#include "Fonts/FreeSans9pt7b.h"

// Includes
#include "Network.h"
#include "secrets.h"
#include "LogSerial.h"

#include <algorithm>
#include <ctime>

// Don't change the following - change the values in secrets.h ---------------
// If you don't have a secrets.h - copy secrets.h.example to secrets.h 
// then change your copy

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;
char calendarURL[] = CALENDAR_URL;

int timeZone = TIMEZONE_OFFSET_GMT;

// Set to 3 to flip the screen 180 degrees
#define ROTATION 1

//---------------------------

// Delay between API calls
#define DELAY_MS 15 * 60 * 1000

// Initiate out Inkplate object
Inkplate display;

// Our networking functions, see Network.cpp for info
Network network;

// Variables for time and raw event info
char date[64];
char *data;
//#define DATA_BUFFER_SIZE 2000000LL
#define DATA_BUFFER_SIZE 100000LL

// Set background of first task to color green
int currentColor = 2;

// Struct for storing calender event info
struct entry
{
    char name[128];
    char time[128];
    char location[128];
    int day = -1;
    int timeStamp;
};

// Here we store calendar entries
int entriesNum = 0;
#define MAX_ENTRIES 100
entry entries[MAX_ENTRIES];

#define DAYS_SHOWN 3

// All our functions declared below setup and loop
void drawInfo();
void drawTime();
void drawGrid();
void getToFrom(char *dst, char *from, char *to, int *day, int *timeStamp);
bool drawEvent(entry *event, int day, int beginY, int maxHeigth, int *heigthNeeded);
int cmp(const void *a, const void *b);
void drawData();

void setup()
{
    Serial.begin(115200);

    data = (char *)ps_malloc(DATA_BUFFER_SIZE);

    // Initial display settings
    display.begin();

    display.clearDisplay();
    display.setRotation(ROTATION);
    display.setTextWrap(false);
    display.setTextColor(0, 7);

    network.begin();

    resetEvents();

    if (    network.getData(calendarURL, DATA_BUFFER_SIZE, data)
         == NETWORK_RC_OK) // Try getting data
    {
        LogSerial_Info("About to start parsing");
        //Get events in form we can draw
        parseDataForEvents(data);
        LogSerial_Info("About to start sorting");
        sortEvents();
        LogSerial_Info("About to start drawing");

        // Drawing all data, functions for that are above
        drawInfo();
        drawGrid();
        drawData();

        // Actually display all data
        display.display();
    }

    // Go to sleep before checking again
    esp_sleep_enable_timer_wakeup(1000L * DELAY_MS);
    esp_deep_sleep_start();
}

void loop()
{
    // Never here
}

// Function for drawing calendar info
void drawInfo()
{
    LogSerial_Verbose1(">>>drawInfo");
    
    // Setting font and color
    display.setTextColor(0, 7);
    display.setFont(&FreeSans12pt7b);
    display.setTextSize(1);

    display.setCursor(20, 20);

    // Find email in raw data
    char temp[64];
    char *start = strstr(data, "X-WR-CALNAME:");

    // If not found return
    if (!start) {
        Serial.println(F("Had bytes of data but found no title"));
        Serial.println(strlen(data));
        return;
    }

    // Find where it ends
    start += 13;
    char *end = strchr(start, '\n');

    strncpy(temp, start, end - start - 1);
    temp[end - start - 1] = 0;

    // Print it
    display.println(temp);
}

// Draw lines in which to put events
void drawGrid()
{
    // upper left and low right coordinates
    int x1 = 3, y1 = 30;
    int x2 = 448 - 3, y2 = 598;

    // header size, for day info
    int header = 30;

    // Columns and rows
    int n = 1, m = 3;

    // Line drawing
    display.drawThickLine(x1, y1 + header, x2, y1 + header, 0, 2.0);
    for (int i = 0; i < n + 1; ++i)
    {
        display.drawThickLine(x1, (int)((float)y1 + (float)i * (float)(y2 - y1) / (float)n), x2,
                              (int)((float)y1 + (float)i * (float)(y2 - y1) / (float)n), 0, 2.0);
    }
    for (int i = 0; i < m + 1; ++i)
    {
        display.drawThickLine((int)((float)x1 + (float)i * (float)(x2 - x1) / (float)m), y1,
                              (int)((float)x1 + (float)i * (float)(x2 - x1) / (float)m), y2, 0, 2.0);
        display.setFont(&FreeSans9pt7b);

        // Display day info using time offset
        char temp[16];
        getDateStringOffset(temp, i * 3600L * 24, false);

        // calculate where to put text and print it
        display.setCursor(17 + (int)((float)x1 + (float)i * (float)(x2 - x1) / (float)m) + 15, y1 + header - 9);
        display.println(temp);
        LogSerial_Verbose1("Column title: %s", temp); //Of the form 'Tue May  2'
    }
}

// Function to draw event
bool drawEvent(entry *event, int day, int beginY, int maxHeigth, int *heigthNeeded)
{
    // Upper left coordintes
    int x1 = 3 + 4 + (440 / 3) * day;
    int y1 = beginY + 3;

    // Setting text font
    display.setFont(&FreeSans12pt7b);

    // Some temporary variables
    int n = 0;
    char line[128];

    // Insert line brakes into setTextColor
    int lastSpace = -100;
    display.setCursor(x1 + 5, beginY + 26);
    for (int i = 0; i < min((size_t)64, strlen(event->name)); ++i)
    {
        // Copy name letter by letter and check if it overflows space given
        line[n] = event->name[i];
        if (line[n] == ' ')
            lastSpace = n;
        line[++n] = 0;

        int16_t xt1, yt1;
        uint16_t w, h;

        // Gets text bounds
        display.getTextBounds(line, 0, 0, &xt1, &yt1, &w, &h);

        // Char out of bounds, put in next line
        if (w > 420 / 3 - 30)
        {
            // if there was a space 5 chars before, break line there
            if (n - lastSpace < 5)
            {
                i -= n - lastSpace - 1;
                line[lastSpace] = 0;
            }

            // Print text line
            display.setCursor(x1 + 5, display.getCursorY());
            display.println(line);

            // Clears line (null termination on first charachter)
            line[0] = 0;
            n = 0;
        }
    }

    // display last line
    display.setCursor(x1 + 5, display.getCursorY());
    display.println(line);

    // Set cursor on same y but change x
    display.setCursor(x1 + 3, display.getCursorY());
    display.setFont(&FreeSans9pt7b);

    // Print time
    // also, if theres a location print it
    if (strlen(event->location) != 1)
    {
        display.println(event->time);

        display.setCursor(x1 + 5, display.getCursorY());

        char line[128] = {0};

        for (int i = 0; i < strlen(event->location); ++i)
        {
            line[i] = event->location[i];
            line[i + 1] = 0;

            int16_t xt1, yt1;
            uint16_t w, h;

            // Gets text bounds
            display.getTextBounds(line, 0, 0, &xt1, &yt1, &w, &h);

            if (w > (442 / 3))
            {
                for (int j = i - 1; j > max(-1, i - 4); --j)
                    line[j] = '.';
                line[i] = 0;
            }
        }

        display.print(line);
    }
    else
    {
        display.print(event->time);
    }

    // Calculating coordinates of text box
    int bx1 = x1 + 2;
    int by1 = y1;
    int bx2 = x1 + 440 / 3 - 7;
    int by2 = display.getCursorY() + 7;

    // Now we know the full height of the text box
    // From here, we print the background of according height and print the text again

    // Fill with color and cycle to next color
    display.fillRect(bx1, by1, 440 / 3 - 7, by2 - by1, currentColor);

    // If the color selected is yellow, print the text in black
    // Otherwise, print it in white
    if (currentColor == 5)
        display.setTextColor(0);
    else
        display.setTextColor(1);

    // Cycle to the next color
    currentColor++;
    if (currentColor == 7)
        currentColor = 2;

    // Upper left coordintes
    x1 = 3 + 4 + (440 / 3) * day;
    y1 = beginY + 3;

    // Setting text font
    display.setFont(&FreeSans12pt7b);

    n = 0;

    // Insert line brakes into setTextColor
    lastSpace = -100;
    display.setCursor(x1 + 5, beginY + 26);
    for (int i = 0; i < min((size_t)64, strlen(event->name)); ++i)
    {
        // Copy name letter by letter and check if it overflows space given
        line[n] = event->name[i];
        if (line[n] == ' ')
            lastSpace = n;
        line[++n] = 0;

        int16_t xt1, yt1;
        uint16_t w, h;

        // Gets text bounds
        display.getTextBounds(line, 0, 0, &xt1, &yt1, &w, &h);

        // Char out of bounds, put in next line
        if (w > 420 / 3 - 30)
        {
            // if there was a space 5 chars before, break line there
            if (n - lastSpace < 5)
            {
                i -= n - lastSpace - 1;
                line[lastSpace] = 0;
            }

            // Print text line
            display.setCursor(x1 + 5, display.getCursorY());
            display.println(line);

            // Clears line (null termination on first charachter)
            line[0] = 0;
            n = 0;
        }
    }

    // display last line
    display.setCursor(x1 + 5, display.getCursorY());
    display.println(line);

    // Set cursor on same y but change x
    display.setCursor(x1 + 3, display.getCursorY());
    display.setFont(&FreeSans9pt7b);

    // Print time
    // also, if theres a location print it
    if (strlen(event->location) != 1)
    {
        display.println(event->time);

        display.setCursor(x1 + 5, display.getCursorY());

        char line[128] = {0};

        for (int i = 0; i < strlen(event->location); ++i)
        {
            line[i] = event->location[i];
            line[i + 1] = 0;

            int16_t xt1, yt1;
            uint16_t w, h;

            // Gets text bounds
            display.getTextBounds(line, 0, 0, &xt1, &yt1, &w, &h);

            if (w > (442 / 3))
            {
                for (int j = i - 1; j > max(-1, i - 4); --j)
                    line[j] = '.';
                line[i] = 0;
            }
        }

        display.print(line);
    }
    else
    {
        display.print(event->time);
    }


    // Draw event rect bounds
    display.drawThickLine(bx1, by1, bx1, by2, 0, 2.0);
    display.drawThickLine(bx1, by2, bx2, by2, 0, 2.0);
    display.drawThickLine(bx2, by2, bx2, by1, 0, 2.0);
    display.drawThickLine(bx2, by1, bx1, by1, 0, 2.0);

    // Set how high is the event
    *heigthNeeded = display.getCursorY() + 12 - y1;

    // Return is it overflowing
    return display.getCursorY() < maxHeigth - 5;
}

void resetEvents(void)
{
    // reset count
    entriesNum = 0;
}

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

// Format event times - converting to timezone offset
// input: from - start time for event in YYYYMMDDTHHMMSSZ e.g. 19970901T130000Z
// input: to   - end time for event in YYYYMMDDTHHMMSSZ e.g. 19970901T130000Z
// output: dst - date string for event e.g. "14:00-16:00"
// output: day - which calendar day event matches 0 - for first day
// output: timestamp - event start in epoch time
// 
void getToFrom(char *dst, char *from, char *to, int *day, int *timeStamp)
{
    struct tm ltm = {0};
    char temp[128];

    LogSerial_Verbose2(">>> getToFrom (will cpy from addresses %ld and %ld)", from, to);

    //Creating in "temp" (a version of "from" in a form strptime accepts)
    strncpy(temp, from, 16);
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
    time_t from_epochtime_local = mktime(&ltm) + (time_t)timeZone * 3600L;

    struct tm from_tm_local;
    gmtime_r(&from_epochtime_local, &from_tm_local);
    strncpy(dst, asctime(&from_tm_local) + 11, 5);

    dst[5] = '-';

    //Creating in "temp" (a version of "to" in a form strptime accepts)
    strncpy(temp, to, 16);
    temp[16] = 0;
    LogSerial_Verbose5("Parsing to: %s", temp);

    // https://github.com/esp8266/Arduino/issues/5141, quickfix
    memmove(temp + 5, temp + 4, 16);
    memmove(temp + 8, temp + 7, 16);
    memmove(temp + 14, temp + 13, 16);
    memmove(temp + 16, temp + 15, 16);
    temp[4] = temp[7] = temp[13] = temp[16] = '-';

    strptime(temp, "%Y-%m-%dT%H-%M-%SZ", &ltm);

    //Add the end output date string (to dst) in form (e.g. add 14:00 to 13:00-)
    
    // TODO: consider suspect timezone handling
    time_t to_epochtime_local = mktime(&ltm) + (time_t)timeZone * 3600L;
    
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
// returns: true if event was included in entrylist
bool parseAllDayEvent(entry *pEvents, int *pEventIndex, int maxEvents, char *dateStart, char *dateEnd)
{
    //convert dateStart/dateEnd to ints 
    //We could compare as strings - but this is easier to reason about and not performance sensitive
    char temp[9];
    strncpy(temp, dateStart, 8);
    temp[8] = '\0';
    long dateStartInt = strtol(temp, NULL, 10);

    strncpy(temp, dateEnd, 8);
    temp[8] = '\0';    
    long dateEndInt   = strtol(dateEnd, NULL, 10);

    if (    dateStartInt < 20000101 || dateStartInt > 22000101 
         || dateEndInt   < 20000101 || dateEndInt   > 22000101  )
    {
        LogSerial_Error("parseAllDayEvent: Event %s has dates out of range: start %ld end %ld",
                                pEvents[*pEventIndex].name, dateStartInt, dateEndInt);
        return false;
    }

    int relevantDays = 0;

    for (int daynum = 0; daynum < DAYS_SHOWN; daynum++)
    {
        //Get day in the form YYYYMMDD then convert to int: dayInt
        //TODO: Consider timezone handling
        time_t dayunixtime = time(nullptr) + (long)timeZone * 3600L +  daynum * 24 * 3600L;
        struct tm day_tm;
        gmtime_r(&dayunixtime, &day_tm);
        strftime(temp, 8, "%Y%m%d", &day_tm);
        long dayInt = strtol(temp, NULL, 10);

        if (dayInt <= dateEndInt) //Check it's not in the past
        {
            if (dayInt >= dateStartInt) //Check it's not in the future
            {
                //We need to show this event in column daynum
                LogSerial_Verbose3("parseAllDayEvent: Event %s is relevant for day %ld : start %d end %ld",
                                  pEvents[*pEventIndex].name, daynum, dateStartInt, dateEndInt);
                relevantDays++;
               
                if (*pEventIndex < maxEvents - 1)
                {
                    //Copy the partial event we are completing to the next slot (in case we need it for the next day)
                    //and complete the slot that we copied
                    memcpy(&pEvents[(*pEventIndex) + 1], &pEvents[*pEventIndex], sizeof(struct entry));
                    pEvents[(*pEventIndex)].day = daynum;
                    strcpy(pEvents[(*pEventIndex)].time, "All Day");

                    //Work out timestamp for midnight at start of this day
                    struct tm midnight_tm = day_tm;

                    midnight_tm.tm_sec = 0;
                    midnight_tm.tm_min = 0;
                    midnight_tm.tm_hour = timeZone;
                    pEvents[(*pEventIndex)].timeStamp = mktime(&midnight_tm);

                    
                    *pEventIndex = *pEventIndex + 1;
                }
                else
                {
                    LogSerial_Error("parseAllDayEvent: Event %s (day %d - start %ld end %ld) - No space in entry list!",
                                    pEvents[*pEventIndex].name, daynum, dateStartInt, dateEndInt);
                }
            }          
        }
    }
    if (dateStartInt > 20230401 && dateStartInt < 20230530)
    {
        LogSerial_Info("parseAllDayEvent: Event %s (start %ld end %ld) - relevant %d days",
                                pEvents[*pEventIndex].name, dateStartInt, dateEndInt, relevantDays);
    }
    else
    {
        LogSerial_Verbose1("parseAllDayEvent: Event %s (start %ld end %ld) - relevant %d days",
                                pEvents[*pEventIndex].name, dateStartInt, dateEndInt, relevantDays);
    }
    return (relevantDays > 0);
}

void parseDataForEvents(char *rawData)
{
    long i = 0;
    long n = strlen(rawData);

    long foundEventsTotal = 0;
    long foundEventsRelevant = 0;
    bool eventRelevant = false;

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
        }
        else
        {
            LogSerial_Warning("Found event without end (position %ld): %s", i, rawData + i);
            end = rawData + strlen(rawData);
        }

        // Find all relevant event data
        char *summarySearch = strstr(rawData + i, "SUMMARY:");
        char *locationSearch = strstr(rawData + i, "LOCATION:");
        char *timeStartSearch = strstr(rawData + i, "DTSTART:");
        char *timeEndSearch = strstr(rawData + i, "DTEND:");
        char *dateStartSearch = strstr(rawData + i, "DTSTART;VALUE=DATE:");
        char *dateEndSearch = strstr(rawData + i, "DTEND;VALUE=DATE:");

        char *summary = NULL;
        char *location = NULL;
        char *timeStart = NULL;
        char *timeEnd = NULL;
        char *dateStart = NULL;
        char *dateEnd = NULL;

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

        LogSerial_Verbose2("Finished finding fields for event %d (so far: relevant %d, total %d)",
                                                 entriesNum, foundEventsRelevant, foundEventsTotal);

        if (summary && summary < end)
        {
            strncpy(entries[entriesNum].name, summary, strchr(summary, '\n') - summary);
            entries[entriesNum].name[strchr(summary, '\n') - summary] = 0;
            LogSerial_Verbose1("Summary: %s", entries[entriesNum].name);
        }
        else
        {
            LogSerial_Unusual("Event with no summary: %.*s", end - rawData - i, rawData + i);
        }

        if (location && location < end)
        {
            strncpy(entries[entriesNum].location, location, strchr(location, '\n') - location);
            entries[entriesNum].location[strchr(location, '\n') - location] = 0;
            LogSerial_Verbose1("Location: %s", entries[entriesNum].location);
        }
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
                    if(parseAllDayEvent(entries, &entriesNum, MAX_ENTRIES, dateStart, dateEnd))
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
          ++foundEventsRelevant;
        }
        ++foundEventsTotal;
    }
    LogSerial_Info("Found %ld relevant events out of %ld",foundEventsRelevant, foundEventsTotal );
}

// Struct event comparison function, by timestamp, used for qsort later on
int cmp(const void *a, const void *b)
{
    entry *entryA = (entry *)a;
    entry *entryB = (entry *)b;

    return (entryA->timeStamp - entryB->timeStamp);
}

void sortEvents(void)
{
    // Sort entries by time
    qsort(entries, entriesNum, sizeof(entry), cmp);
}

// Main data drawing data
void drawData()
{
    // Events displayed and overflown counters
    int columns[3] = {0};
    bool clogged[3] = {0};
    int cloggedCount[3] = {0};

    // Displaying events one by one
    for (int i = 0; i < entriesNum; ++i)
    {
        // If column overflowed just add event to not shown
        if (entries[i].day != -1 && clogged[entries[i].day])
            ++cloggedCount[entries[i].day];
        if (entries[i].day == -1 || clogged[entries[i].day])
            continue;

        // We store how much height did one event take up
        int shift = 0;
        bool s = drawEvent(&entries[i], entries[i].day, columns[entries[i].day] + 64, 600 - 4, &shift);

        columns[entries[i].day] += shift;

        // If it overflowed, set column to clogged and add one event as not shown
        if (!s)
        {
            ++cloggedCount[entries[i].day];
            clogged[entries[i].day] = 1;
        }
    }

    // Display not shown events info
    for (int i = 0; i < 3; ++i)
    {
        if (clogged[i])
        {
            // Draw notification showing that there are more events than drawn ones
            display.fillRoundRect(6 + i * (442 / 3), 600 - 24, (442 / 3) - 5, 20, 10, 0);
            display.setCursor(10, 600 - 6);
            display.setTextColor(7, 0);
            display.setFont(&FreeSans9pt7b);
            display.print(cloggedCount[i]);
            display.print(" more events");
        }
    }
}
