/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
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
