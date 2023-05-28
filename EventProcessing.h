#ifndef EVENTPROCESSING_H
#define EVENTPROCESSING_H

#include <stdint.h>
#include "entry.h"

typedef struct ProcessingRule_t {
     uint64_t MatchType;
     const char *MatchString; 
     uint64_t Result; 
     uint64_t ResultArg;
} ProcessingRule_t;

#define INKYR_MATCH_END                       0   //Signifies the end of processing rules
#define INKYR_MATCH_CONTAINS                  1   //If MatchString is (using case insensitive compare) in event summary, description, location
#define INKYR_MATCH_DOES_NOT_CONTAIN          2   //If MatchString is (using case insensitive compare) in event summary, description, location
#define INKYR_MATCH_SUMMARY_EQUALS_STRIP      3   //If MatchString is (using case insensitive compare) equal (aside from leading trailing whitespace) to event summary

#define INKYR_RESULT_DISCARD                  1
#define INKYR_RESULT_SETCOLOUR                2
#define INKYR_RESULT_SETCOLOR                 INKYR_RESULT_SETCOLOUR


uint32_t runEventMatchRules(const ProcessingRule_t *pEventRules, entry_t *entryptr,  const char *entrydesc, const char *recurRule);

#endif