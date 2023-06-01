/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
   
   It shows selected events from multiple calendars
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
#include "InkyCalInternal.h"
#include "Network.h"
#include "entry.h"
#include "Calendar.h"
#include "secrets.h"
#include "LogSerial.h"

#include <algorithm>
#include <ctime>

// Don't change the following - change the values in secrets.h ---------------
// If you don't have a secrets.h - copy secrets.h.example to secrets.h 
// then change your copy

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;

char timezoneString[] = POSIX_TIMEZONE;

// Set to 3 to flip the screen 180 degrees
#define ROTATION 3

//---------------------------

// Delay (milliseconds) between calendar updates
#define DELAY_MS UINT64_C(60 * 60 * 1000)

// Initiate out Inkplate object
Inkplate display;

// Our networking functions, see Network.cpp for info
Network network;

// Variables for time and raw event info
char date[64];
char *data;
//#define DATA_BUFFER_SIZE 2000000LL
#define DATA_BUFFER_SIZE 100000LL

//If we are asked for random colours for entries we cycle through 
//the colours (skipping black and white)
int8_t currentColor = INKY_EVENT_COLOUR_GREEN;

//These are increased with logProblem (and use by drawInfo())
uint64_t loggedWarnings = 0;
uint64_t loggedErrors   = 0;
uint64_t loggedFatals   = 0;

uint32_t numCalendars = 0;

//Number of days shown - much of display code not yet modified to use this
// and hardcodes 3
#define DAYS_SHOWN 3 


// All our functions declared below setup and loop
bool parseAllCalendars();
void drawInfo();
void drawGrid();
bool drawEvent(entry_t *event, int day, int beginY, int maxHeigth, int *heigthNeeded);
void drawData();

void setup()
{
    Serial.begin(115200);

    // Initial display settings
    display.begin();

    display.clearDisplay();
    display.setRotation(ROTATION);
    display.setTextWrap(false);
    display.setTextColor(0, 7);

    network.begin(timezoneString);

    resetEntries();

    time_t calendarStart = time(nullptr);

    //Uncomment to show calendar at a fixed time - not "now"
    //struct tm sometime={0};
    //char timetemp[]="2023-10-27T11:11:00";
    //strptime(timetemp, "%Y-%m-%dT%H:%M:%S", &sometime);
    //sometime.tm_isdst = -1;
    //calendarStart = mktime(&sometime);

    setCalendarRange(calendarStart, 3);

    if ( parseAllCalendars() ) // Try getting data
    {
        LogSerial_Info("About to start sorting");
        SortEntries();
        LogSerial_Info("About to start drawing");

        // Drawing all data, functions for that are above
        drawInfo();
        drawGrid();
        drawData();

        // Actually display all data
        display.display();
    }

    // Enable wakeup from deep sleep on gpio 36 (wake button)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, 0);

    // Go to sleep before checking again
    esp_sleep_enable_timer_wakeup(UINT64_C(1000) * DELAY_MS);
    esp_deep_sleep_start();
}

void loop()
{
    // Never here
}

bool parseAllCalendars()
{
    bool allok = true;
    Calendar_t *pCal = &Calendars[0];

    while (pCal->url != NULL && allok)
    { 
        CalendarParsingContext_t context;
        context.pCal = pCal;
        context.calRelevantEvents = 0;
        context.calEvents = 0;

        numCalendars++;        

        int networkrc =  network.getData(pCal->url, DATA_BUFFER_SIZE, 
                              parsePartialDataForEvents, &context);

        if (networkrc != NETWORK_RC_OK)
        {
            LogSerial_FatalError("Failed (rc=%d) in network read of %s",
                  networkrc, pCal->url);
            logProblem(INKY_SEVERITY_FATAL);
            allok = false;
        }
        pCal++;
    }
  
    return allok;
}

// Function for drawing calendar info
void drawInfo()
{
    LogSerial_Verbose1(">>>drawInfo");
    
    // Setting font and color
    if (loggedWarnings == loggedErrors == loggedFatals == 0)
    {
        display.setTextColor(INKY_EVENT_COLOUR_BLACK, 7);
    }
    else
    {
        display.setTextColor(INKY_EVENT_COLOUR_RED, 7);      
    }
    display.setFont(&FreeSans12pt7b);
    display.setTextSize(1);

    display.setCursor(20, 20);

    char timestr[24];
    uint32_t charsUsed = getTimeStringNow(timestr, 22);

    if (charsUsed == 0)
    {
        LogSerial_Error("Failed to get time string");
        logProblem(INKY_SEVERITY_ERROR);
    }

    char statsstr[24];

    if (loggedWarnings == loggedErrors == loggedFatals == 0)
    {

      snprintf (statsstr,24, "c: %" PRIu32 " e: %" PRIu64 "/%" PRIu64 "", 
                numCalendars, getRelevantEventCount(), getTotalEventCount());
    }
    else
    {
      snprintf (statsstr,24, "W: %" PRIu64 " E: %" PRIu64 "!", loggedWarnings, loggedErrors+loggedFatals);
    }

    bool buttonPressed = false;

    esp_sleep_wakeup_cause_t wakeup_reason;

    //This always seems to give reason 0 - no big deal - but something to investigate?
    wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0)
    {
        LogSerial_Info("Wake up = button");
        buttonPressed = true;
    }
    else
    {
        LogSerial_Info("Wake up reason: %u", wakeup_reason);      
    }

    char title[48];
    snprintf(title, 48, "%s: %s (%s)",
          (buttonPressed ? "But": "Upd"),
          timestr, statsstr);
    // Print it
    display.println(title);
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
        getDateStringOffsetDays(temp, i, false);

        // calculate where to put text and print it
        display.setCursor(17 + (int)((float)x1 + (float)i * (float)(x2 - x1) / (float)m) + 15, y1 + header - 9);
        display.println(temp);
        LogSerial_Verbose1("Column title: %s", temp); //Of the form 'Tue May  2'
    }
}

// Function to draw event
bool drawEvent(entry_t *event, int day, int beginY, int maxHeigth, int *heigthNeeded)
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

    // Fill with colour and cycle to next color
    uint16_t boxColour;
    uint16_t fgColour;
    
    if (event->bgColour != INKY_EVENT_COLOUR_RANDOM)
    {
        boxColour = event->bgColour;
        fgColour  = event->fgColour;
    }
    else
    {
        event->bgColour = currentColor;

        // If the color selected is yellow, print the text in black
        // Otherwise, print it in white
        if (boxColour == INKY_EVENT_COLOUR_YELLOW)
            fgColour = INKY_EVENT_COLOUR_BLACK;
        else
            fgColour = INKY_EVENT_COLOUR_WHITE;

        // Cycle to the next color
        currentColor++;
        if (currentColor > INKY_EVENT_COLOUR_ORANGE)
            currentColor = INKY_EVENT_COLOUR_GREEN;
    }

    display.fillRect(bx1, by1, 440 / 3 - 7, by2 - by1, boxColour);
    display.setTextColor(fgColour);

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

void logProblem(uint32_t severity)
{
    if (severity == INKY_SEVERITY_WARNING)
    {
        loggedWarnings++;
    }
    else if (severity == INKY_SEVERITY_ERROR)
    {
        loggedErrors++;
    }
    else if (severity == INKY_SEVERITY_FATAL)
    {
        loggedFatals++;
    }
    else
    {
        LogSerial_Error("logProblem called with unknown severity % " PRIu32 ".", severity);
        loggedErrors++;
    }
}
