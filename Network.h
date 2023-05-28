/*
Network.h
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

#include "Arduino.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// To get timeZone from main file
extern int timeZone;

// Wifi ssid and password
extern char ssid[];
extern char pass[];

#ifndef NETWORK_H
#define NETWORK_H

#define NETWORK_RC_OK         0
#define NETWORK_RC_BUFFULL   -1
#define NETWORK_RC_PARSEFAIL -2 //function parsing the streamed data failed

//As we download data we send it in chunks to the the following function:
//First arg: data to parse
//Second arg: oparsingContext
//
//returns: pointer to start of unparsed data or NULL for error
typedef char *dataParsingFn_t(char *, void *);

// All functions defined in Network.cpp

class Network
{
  public:
    // Functions we can access in main file
    void begin();
    int getData(const char *url, size_t maxbufsize, dataParsingFn_t parser, void *parsingContext);

  private:
    // Functions called from within our class
    void setTime();
};

#endif
