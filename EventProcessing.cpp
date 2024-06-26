/*
   This program is free software: you can redistribute it and/or modify it under 
   the terms of the GNU General Public License as published by the Free Software 
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
*/
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>

#include "InkyCalInternal.h"
#include "EventProcessing.h"
#include "entry.h"
#include "LogSerial.h"


//returns chars at start of string skippable as part of a fold
size_t countFoldChars(const char *str)
{
    size_t charsSkipped = 0;
    while(1)
    {
        //check for CRLF+{{space or tab}
        if(    (*str == '\r') && (*(str+1) == '\n') 
            && ( (*(str+2) == ' ') || ((*str+2) == '\t')) )
        {
            str += 3;
            charsSkipped = 3;
        }
        //check for LF+{{space or tab}
        else if (   (*str == '\n') 
                 && ( (*(str+1) == ' ') || ((*str+1) == '\t')) )
        {
            str += 2;
            charsSkipped = 2;
        }
        else
        {
            //Nothing else to do
            break;
        }
    }

    return charsSkipped;
}


//return true if A has B as a (case insensitve) prefix (skipping over fold chars in A)
bool strprefixskipfold(const char *A, const char *B)
{
    while (*B != '\0')
    {
        size_t skippableChars = countFoldChars(A);
        A += skippableChars;

        if (*A == *B)
        {
            A++;
            B++;
        }
        else
        {
            return false;
        }

    }
    return true;
}
//Does case insensitive search in a string with given length for the needle
//It skips over [CR]LF+{space or tab} in haystack 
static bool scancaseFoldedString(const char *foldedhaystack, size_t haystacklen, const char *needle)
{
    const char *haystackpos = foldedhaystack;
    size_t needlelen = strlen(needle);
    size_t remainingChars = haystacklen;

    while (remainingChars >= needlelen && *haystackpos != '\0')
    {
        if (*haystackpos == *needle)
        {
            if (strprefixskipfold(haystackpos, needle))
            {
                return true;
            }
        }
        size_t charsSkippable = countFoldChars(haystackpos);

        if (charsSkippable > 0)
        {
            if (charsSkippable >= remainingChars)
            {
                remainingChars -= charsSkippable;
                haystackpos += charsSkippable;
            }
            else
            {
                remainingChars = 0;
            }
        }
        else
        {
            //Move to next char
            haystackpos++;
            remainingChars--;
        }
    }

    return false;
}

//Does case insensitive search
static bool eventContains(entry_t *entryptr, const char *entrydesc, size_t descLen, const char *searchString)
{
    bool match = false;
  
    match = (strcasestr(entryptr->name, searchString) != NULL);

    if (!match)
    {
        match = (strcasestr(entryptr->location, searchString) != NULL);    
    }
  
    if (!match)
    {
        match = scancaseFoldedString(entrydesc, descLen, searchString);
    }

    return match; 
}

//Does case insensitive search
static bool stringsEqualsStrip(const char *haystack, const char *needle)
{
    bool match = true;

    LogSerial_Verbose2("stringsEqualsStrip - comparing  %s and %s", haystack, needle);

    while(isspace(haystack[0]))
    {
        haystack++;
    }

    while(isspace(needle[0]))
    {
        needle++;
    }

    while(tolower(haystack[0]) == tolower(needle[0])
           && haystack[0] != '\0')
    {
        haystack++;
        needle++;
    }

    //If they match we've scanned through all the non-whitespace characters 
    // - any other type of character indicates a material difference
    //If we scan through trailing whitespace - if there was complete match
    // - then we will hit bull-terminator
    while(isspace(haystack[0]))
    {
        haystack++;
    }

    while(isspace(needle[0]))
    {
        needle++;
    }

    if (haystack[0] != '\0' || needle[0] != '\0')
    {
        match = false;
    }

    if (match)
    {
        LogSerial_Verbose2("found match!");
    }
    return match;
}

uint32_t runEventMatchRules(const ProcessingRule_t *pEventRules, entry_t *entryptr,
                           const char *entrydesc, size_t desclen, 
                           const char *recurRule)
{
    uint32_t eventOutcome = 0; //No action

    while (pEventRules->MatchType != INKYR_MATCH_END)
    {
        bool ruleMatches = false;

        switch(pEventRules->MatchType)
        {
          case INKYR_MATCH_CONTAINS:
              ruleMatches = eventContains(entryptr, entrydesc, desclen, pEventRules->MatchString);
              break;

          case INKYR_MATCH_DOES_NOT_CONTAIN:
              ruleMatches = !eventContains(entryptr, entrydesc, desclen, pEventRules->MatchString);
              break;

          case INKYR_MATCH_SUMMARY_EQUALS_STRIP:
              ruleMatches = stringsEqualsStrip(entryptr->name, pEventRules->MatchString);
              break;

          default:
              LogSerial_Error("Unknown Event Rule type %" PRIu32, pEventRules->MatchType);
              logProblem(INKY_SEVERITY_ERROR);              
        }

        if (ruleMatches)
        {
            switch (pEventRules->Result)
            {
                case INKYR_RESULT_DISCARD:
                    eventOutcome = INKYR_RESULT_DISCARD;
                    break;
                
                case INKYR_RESULT_SETCOLOUR:
                    entry_SetColour(entryptr, pEventRules->ResultArg);
                    break;

                case INKYR_RESULT_SETSORTTIE:
                    entryptr->sortTieBreak = (int8_t)pEventRules->ResultArg;
                    break;
            }       
        }

        if (eventOutcome == INKYR_RESULT_DISCARD)
        {
            //We know we are going to throw this event away - no point processing further
            break;
        }
        pEventRules++;
    }
    return eventOutcome;
}
