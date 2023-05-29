#include "InkyCalInternal.h"
#include "EventProcessing.h"
#include "entry.h"
#include "LogSerial.h"

#include <string.h>

//Does case insensitive search
static bool eventContains(entry_t *entryptr, const char *entrydesc, const char *searchString)
{
    bool match = false;
  
    match = (strcasestr(entryptr->name, searchString) != NULL);

    if (!match)
    {
        match = (strcasestr(entryptr->location, searchString) != NULL);    
    }
  
    if (!match)
    {
        match = (strcasestr(entrydesc, searchString) != NULL);    
    }

    return match; 
}

//Does case insensitive search
static bool stringsEqualsStrip(const char *haystack, const char *needle)
{
    bool match = true;

    LogSerial_Verbose2("stringsEqualsStrip - %s and %s", haystack, needle);

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

uint32_t runEventMatchRules(const ProcessingRule_t *pEventRules, entry_t *entryptr,  const char *entrydesc, const char *recurRule)
{
    uint32_t eventOutcome = 0; //No action

    while (pEventRules->MatchType != INKYR_MATCH_END)
    {
        bool ruleMatches = false;

        switch(pEventRules->MatchType)
        {
          case INKYR_MATCH_CONTAINS:
              ruleMatches = eventContains(entryptr, entrydesc, pEventRules->MatchString);
              break;

          case INKYR_MATCH_DOES_NOT_CONTAIN:
              ruleMatches = !eventContains(entryptr, entrydesc, pEventRules->MatchString);
              break;

          case INKYR_MATCH_SUMMARY_EQUALS_STRIP:
              ruleMatches = stringsEqualsStrip(entryptr->name, pEventRules->MatchString);
              break;

          default:
              LogSerial_Error("Unknown Event Rule type %u", pEventRules->MatchType);
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