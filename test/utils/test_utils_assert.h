/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
*/

/* This file is based in large part on the file with the same name in Eclipse Amlen */

#ifndef TEST_UTILS_ASSERT_H
#define TEST_UTILS_ASSERT_H

#include <signal.h>
#include <string.h>
#include <assert.h>

#include "LogSerial.h"

#define test_log(msg, ...)  LogSerial_LogImpl(LOGSERIAL_LEVEL_FATALERROR, msg, ##__VA_ARGS__)

#ifdef RUNNING_UNDER_ASAN
//If we're running under ASAN, pause the process as we won't process a useful core
static inline void asan_pause(void)
{
    //Want to get to the pause ASAP... so use puts rather than printf)
    puts("ASAN: Pausing... (use gdb or fg or kill -s SIGCONT to continue)\n");

    //pause us...
    raise(SIGTSTP);

    //Ensure the signal arrived
    sleep(1);

    //If we got here we're running again
    printf("Running again\n");
}
#else
//If we're not running under ASAN we can go and produe a core as usual... no need to pause
#define asan_pause()
#endif


#define TEST_ASSERT_EQUAL_FORMAT(a, b, format) \
        { if( (a) != (b)) { \
             test_log( "TEST_ASSERT ("#a ")=" format " == (" #b ")=" format " on line %u file: %s\n", \
                             (a),(b), __LINE__,__FILE__); \
             asan_pause(); \
             assert((a) == (b)); \
             __builtin_trap(); \
          } \
        }

#define TEST_ASSERT_EQUAL(a, b) TEST_ASSERT_EQUAL_FORMAT(a, b, "%d")
#define TEST_ASSERT_PTR_NULL(p) TEST_ASSERT_EQUAL_FORMAT(p, NULL, "%p")

#define TEST_ASSERT_NOT_EQUAL_FORMAT(a, b, format) \
        { if( (a) == (b)) { \
             test_log( "TEST_ASSERT ("#a ")=" format " != (" #b ")=" format " on line %u file: %s\n", \
                             (a),(b), __LINE__,__FILE__); \
             asan_pause(); \
             assert((a) != (b)); \
             __builtin_trap(); \
          } \
        }

#define TEST_ASSERT_NOT_EQUAL(a, b) TEST_ASSERT_NOT_EQUAL_FORMAT(a, b, "%d")
#define TEST_ASSERT_PTR_NOT_NULL(p) TEST_ASSERT_NOT_EQUAL_FORMAT(p, NULL, "%p")

#define TEST_ASSERT_GREATER_THAN_FORMAT(a, b, format) \
        { if( (a) <= (b)) { \
             test_log( "TEST_ASSERT ("#a ")=" format " > (" #b ")=" format " on line %u file: %s\n", \
                             (a),(b), __LINE__,__FILE__); \
             asan_pause(); \
             assert((a) != (b)); \
             __builtin_trap(); \
          } \
        }

#define TEST_ASSERT_GREATER_THAN(a, b) TEST_ASSERT_GREATER_THAN_FORMAT(a, b, "%d")

#define TEST_ASSERT_GREATER_THAN_OR_EQUAL_FORMAT(a, b, format) \
        { if( (a) < (b)) { \
             test_log( "TEST_ASSERT ("#a ")=" format " >= (" #b ")=" format " on line %u file: %s\n", \
                             (a),(b), __LINE__,__FILE__); \
             asan_pause(); \
             assert((a) != (b)); \
             __builtin_trap(); \
          } \
        }

#define TEST_ASSERT_GREATER_THAN_OR_EQUAL(a, b) TEST_ASSERT_GREATER_THAN_OR_EQUAL_FORMAT(a, b, "%d")

#define TEST_ASSERT(_exp, _format, ...) \
       {  if (!(_exp)) { \
                test_log("ASSERT(%s) on line %u file: %s: " _format "\n", #_exp, __LINE__,__FILE__, ##__VA_ARGS__); \
                asan_pause(); \
                assert(_exp); \
                __builtin_trap(); \
          } \
       }

#define TEST_ASSERT_STRINGS_EQUAL(a, b) \
        { if( strcmp((a), (b)) != 0) { \
             test_log( "TEST_ASSERT ("#a ")='%s' == (" #b ")='%s' on line %u file: %s\n", \
                             (a),(b), __LINE__,__FILE__); \
             asan_pause(); \
             assert(false); \
             __builtin_trap(); \
          } \
        }

#endif