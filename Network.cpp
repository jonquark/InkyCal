/*
Network.cpp
Inkplate 6COLOR Arduino library
David Zovko, Borna Biro, Denis Vajak, Zvonimir Haramustek @ Soldered
September 24, 2020
https://github.com/e-radionicacom/Inkplate-6-Arduino-library

For support, please reach over forums: forum.e-radionica.com/en
For more info about the product, please check: www.inkplate.io

This code is released under the GNU Lesser General Public License v3.0: https://www.gnu.org/licenses/lgpl-3.0.en.html
Please review the LICENSE file included with this example.
If you have any questions about licensing, please contact techsupport@e-radionica.com
Distributed as-is; no warranty is given.
*/

#include "Network.h"
#include "LogSerial.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

void Network::begin()
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
    setTime();
}

// Function to get all data from web
//
// Currently restarts device on network errors e.g. no WIFI(!)
//
// returns NETWORK_RC_OK (0) on sucess
//         NETWORK_RC_BUFFULL if buffer was too small
//         Positive integer: HTTP status code
//
int Network::getData(const char *url, size_t maxbufsize, char *databuf)
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
        long n = 0;

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
                charsInBatch++;

                if (n < maxbufsize -1 )
                {
                    databuf[n++] = http.getStream().read();
                }
                else
                {
                    rc = NETWORK_RC_BUFFULL;
                }
            }

            now = millis();

            if (charsInBatch > 0)
            {
                timeoutStart = now;
            }

            if (now - lastprogressreport > 2 * 1000)
            {
                LogSerial_Info("So far, received bytes of data: %d", n);
                lastprogressreport = now;
            }

        }
        LogSerial_Info("In total, received bytes of data: %d", n);
        databuf[n++] = 0;
        LogSerial_Verbose3("Received data:\n%s", databuf);

        if ( n > 100)
        {
            LogSerial_Verbose3("Last 100 bytes of data:\n%s", databuf + (n-100));
        }
    }
    else
    {
        LogSerial_Error("Received HTTP Code %d", httpCode);
        rc = httpCode;
    }

    // end http
    http.end();

    return rc;
}

void Network::setTime()
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

    // Used to store time info
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    Serial.print(F("Current time: "));
    Serial.print(asctime(&timeinfo));
}
