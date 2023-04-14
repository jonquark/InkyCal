#ifndef LOGSERIAL_H
#define LOGSERIAL_H

#include "Arduino.h"

#define LOGSERIAL_LEVEL_SILENT     0
#define LOGSERIAL_LEVEL_ERROR     20
#define LOGSERIAL_LEVEL_INFO      50
#define LOGSERIAL_LEVEL_VERBOSE   80

//Change this line to choose logging level
#define LOGSERIAL_LOGGING_LEVEL LOGSERIAL_LEVEL_VERBOSE

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_ERROR
#define LogSerial_Error(msg, ...) LogSerial_LogImpl(LOGSERIAL_LEVEL_ERROR, msg, ##__VA_ARGS__)
#else
#define LogSerial_Error(msg, ...)
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_INFO
#define LogSerial_Info(msg, ...)  LogSerial_LogImpl(LOGSERIAL_LEVEL_INFO, msg, ##__VA_ARGS__)
#else
#define LogSerial_Info(msg, ...)
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_VERBOSE
#define LogSerial_Verbose(msg, ...)  LogSerial_LogImpl(LOGSERIAL_LEVEL_VERBOSE, msg, ##__VA_ARGS__)
#else
#define LogSerial_Verbose(msg, ...) 
#endif

void LogSerial_LogImpl(int loglevel, const char* msg, ...);

#endif