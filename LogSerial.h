#ifndef LOGSERIAL_H
#define LOGSERIAL_H

#include "Arduino.h"

#define LOGSERIAL_LEVEL_SILENT         0
#define LOGSERIAL_LEVEL_FATALERROR    10
#define LOGSERIAL_LEVEL_ERROR         20
#define LOGSERIAL_LEVEL_WARNING       30
#define LOGSERIAL_LEVEL_UNUSUAL       40
#define LOGSERIAL_LEVEL_INFO          50
#define LOGSERIAL_LEVEL_VERBOSE1      70
#define LOGSERIAL_LEVEL_VERBOSE2      80
#define LOGSERIAL_LEVEL_VERBOSE3      90
#define LOGSERIAL_LEVEL_VERBOSE4      95
#define LOGSERIAL_LEVEL_VERBOSE5     100


//Change this line to choose logging level
#define LOGSERIAL_LOGGING_LEVEL LOGSERIAL_LEVEL_INFO
#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_FATALERROR
#define LogSerial_FatalError(msg, ...) LogSerial_LogImpl(LOGSERIAL_LEVEL_FATALERROR, msg, ##__VA_ARGS__)
#else
#define LogSerial_FatalError(msg, ...)
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_ERROR
#define LogSerial_Error(msg, ...) LogSerial_LogImpl(LOGSERIAL_LEVEL_ERROR, msg, ##__VA_ARGS__)
#else
#define LogSerial_Error(msg, ...)
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_WARNING
#define LogSerial_Warning(msg, ...) LogSerial_LogImpl(LOGSERIAL_LEVEL_WARNING, msg, ##__VA_ARGS__)
#else
#define LogSerial_Warning(msg, ...)
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_UNUSUAL
#define LogSerial_Unusual(msg, ...) LogSerial_LogImpl(LOGSERIAL_LEVEL_UNUSUAL, msg, ##__VA_ARGS__)
#else
#define LogSerial_Unusual(msg, ...)
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_INFO
#define LogSerial_Info(msg, ...)  LogSerial_LogImpl(LOGSERIAL_LEVEL_INFO, msg, ##__VA_ARGS__)
#else
#define LogSerial_Info(msg, ...)
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_VERBOSE1
#define LogSerial_Verbose1(msg, ...)  LogSerial_LogImpl(LOGSERIAL_LEVEL_VERBOSE1, msg, ##__VA_ARGS__)
#else
#define LogSerial_Verbose1(msg, ...) 
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_VERBOSE2
#define LogSerial_Verbose2(msg, ...)  LogSerial_LogImpl(LOGSERIAL_LEVEL_VERBOSE2, msg, ##__VA_ARGS__)
#else
#define LogSerial_Verbose2(msg, ...) 
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_VERBOSE3
#define LogSerial_Verbose3(msg, ...)  LogSerial_LogImpl(LOGSERIAL_LEVEL_VERBOSE3, msg, ##__VA_ARGS__)
#else
#define LogSerial_Verbose3(msg, ...) 
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_VERBOSE4
#define LogSerial_Verbose4(msg, ...)  LogSerial_LogImpl(LOGSERIAL_LEVEL_VERBOSE4, msg, ##__VA_ARGS__)
#else
#define LogSerial_Verbose4(msg, ...) 
#endif

#if LOGSERIAL_LOGGING_LEVEL >= LOGSERIAL_LEVEL_VERBOSE5
#define LogSerial_Verbose5(msg, ...)  LogSerial_LogImpl(LOGSERIAL_LEVEL_VERBOSE5, msg, ##__VA_ARGS__)
#else
#define LogSerial_Verbose5(msg, ...) 
#endif

void LogSerial_LogImpl(int loglevel, const char* msg, ...);

#endif