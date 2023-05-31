/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
*/

#include "InkyCalInternal.h"
#include "Network.h"
#include "LogSerial.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

//Try to parse data only when we have at least this many bytes
#define INKY_NETWORK_MINIMUM_CHUNK_SIZE 50000

void Network::begin(const char *timeZoneString)
{
    // Initiating wifi, like in BasicHttpClient example
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    int cnt = 0;
    Serial.print(F("Waiting for WiFi to connect..."));
    while ((WiFi.status() != WL_CONNECTED))
    {
        Serial.print(F("."));
        delay(1000);
        ++cnt;

        WiFi.reconnect();
        delay(5000);

        if (cnt == 10)
        {
            Serial.println("Can't connect to WIFI, restarting");
            delay(100);
            ESP.restart();
        }
    }
    Serial.println(F(" connected"));

    // Find internet time
    setTime(timeZoneString);
}

// Function to get all data from web
//
// Currently restarts device on network errors e.g. no WIFI(!)
//
// returns NETWORK_RC_OK (0) on sucess
//         NETWORK_RC_BUFFULL if buffer was too small
//         Positive integer: HTTP status code
//
int Network::getData(const char *url, size_t maxbufsize,  dataParsingFn_t parser, void *parsingContext)
{
    // Variable to store fail
    int rc = NETWORK_RC_OK;
    String urlstr(url);

    // If not connected to wifi reconnect wifi
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();

        delay(5000);

        int cnt = 0;
        Serial.println(F("Waiting for WiFi to reconnect..."));
        while ((WiFi.status() != WL_CONNECTED))
        {
            // Prints a dot every second that wifi isn't connected
            Serial.print(F("."));
            delay(1000);
            ++cnt;

            WiFi.reconnect();
            delay(5000);

            if (cnt == 10)
            {
                Serial.println("Can't connect to WIFI, restart initiated.");
                delay(100);
                ESP.restart();
            }
        }
    }
    LogSerial_Info("Preparing to make http request to %s... wifi is connected", url);

    // Http object used to make get request
    HTTPClient http;

    http.getStream().setTimeout(10);
    http.getStream().flush();

    // Begin http by passing url to it
    http.begin(urlstr);

    delay(300);

    // Actually do request
    int httpCode = http.GET();

    if (httpCode == 200)
    {
        uint64_t n = 0;
        //ps_malloc allocs special "psram" - separate external memory
        char *databuf = (char *)ps_malloc(maxbufsize);
        uint64_t totalReceived = 0;
        uint64_t totalParsed   = 0;


        LogSerial_Info("http size (according to Content-Length)  is: %d", http.getSize());

        unsigned long timeoutStart = millis();
        unsigned long now = timeoutStart;
        unsigned long lastprogressreport = now;
        
        while (    (http.connected() || http.getStream().available())
                && (rc == NETWORK_RC_OK) 
                && (now - timeoutStart < 10*1000))
        {
            long charsInBatch = 0;

            while (    http.getStream().available()
                    && (rc == NETWORK_RC_OK) 
                    && (charsInBatch < 500) )
            {
                if (n < maxbufsize -1 )
                {
                    databuf[n++] = http.getStream().read();
                    charsInBatch++;
                }
                else
                {
                    rc = NETWORK_RC_BUFFULL;
                }
            }

            now = millis();

            if (charsInBatch > 0)
            {
                totalReceived += charsInBatch;
                timeoutStart = now;
            }

            if (now - lastprogressreport > 2 * 1000)
            {
                LogSerial_Info("So far, received bytes of data: %" PRIu64, totalReceived);
                lastprogressreport = now;
            }

            if (  rc == NETWORK_RC_BUFFULL
                ||( n > INKY_NETWORK_MINIMUM_CHUNK_SIZE && rc == NETWORK_RC_OK))
            {   
                databuf[n] = 0;
                char *unparseddata = parser(databuf, parsingContext);

                if (unparseddata == NULL)
                {
                    LogSerial_FatalError("Failed to parse %s", url);
                    logProblem(INKY_SEVERITY_FATAL);
                    rc = NETWORK_RC_PARSEFAIL;                  
                }
                else if (unparseddata != databuf)
                {
                    uint64_t justparsed = (unparseddata - databuf);
                    totalParsed += justparsed;
                    n -= justparsed;
                    memmove(databuf, unparseddata, n);
                    rc = NETWORK_RC_OK;
                }
            }
        }
        LogSerial_Info("In total, received bytes of data: %" PRIu64, totalReceived);
        databuf[n++] = 0;
        LogSerial_Verbose3("Received data:\n%s", databuf);

        if ( n > 100)
        {
            LogSerial_Verbose3("Last 100 bytes of data:\n%s", databuf + (n-100));
        }

        if (n > 1 && rc == NETWORK_RC_OK)
        {
            //Parse last data
            char *unparseddata = parser(databuf, parsingContext);

            if (unparseddata == NULL)
            {
                LogSerial_FatalError("Failed to do final parse of %s", url);
                logProblem(INKY_SEVERITY_FATAL);
                rc = NETWORK_RC_PARSEFAIL;                  
            }
            else if (unparseddata != databuf)
            { 
                uint64_t justparsed = (unparseddata - databuf);
                totalParsed += justparsed;
                n -= justparsed;
                memmove(databuf, unparseddata, n);
                LogSerial_Info("Left over %" PRIu64 " bytes of data after final parse: %s", n, databuf); 
            }
        }
        LogSerial_Info("In total, parsed bytes of data: %" PRIu64, totalParsed);
        free(databuf);
    }
    else
    {
        LogSerial_Error("Received HTTP Code %d", httpCode);
        logProblem(INKY_SEVERITY_ERROR);
        rc = httpCode;
    }

    // end http
    http.end();

    return rc;
}

void Network::setTime(const char *timeZoneString)
{
    // Used for setting correct time
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print(F("Waiting for NTP time sync: "));
    time_t nowSecs = time(nullptr);
    while (nowSecs < 8 * 3600 * 2)
    {
        delay(500);
        Serial.print(F("."));
        yield();
        nowSecs = time(nullptr);
    }

    Serial.println();

    //Set the Timezone
    //Timezone handling based on:
    //  https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
    setenv("TZ", timeZoneString ,1);
    tzset();    

    // Used to store time info
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    char temp[26];
    Serial.print(F("Current UTC time: "));
    Serial.print(asctime_r(&timeinfo, temp));

    localtime_r(&nowSecs, &timeinfo);

    Serial.print(F("Current local time: "));
    Serial.print(asctime_r(&timeinfo, temp));
}
