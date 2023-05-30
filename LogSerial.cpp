/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
*/

#include "LogSerial.h"

#define LOGSERIAL_BUFSIZE 256

void LogSerial_LogImpl(int loglevel, const char* msg, ...)
{
    if (loglevel <=  LOGSERIAL_LOGGING_LEVEL)
    {
        char buff[LOGSERIAL_BUFSIZE];
        va_list argptr;

        va_start(argptr, msg);
        int bytesout = vsnprintf(buff, LOGSERIAL_BUFSIZE-1, msg, argptr);
        va_end(argptr);

        buff[LOGSERIAL_BUFSIZE-1] = '\0';

        if (bytesout < LOGSERIAL_BUFSIZE-1)
        {
            Serial.println(buff);
        }
        else
        {
          Serial.print(buff);
          Serial.println("...");
        }
    }
}
