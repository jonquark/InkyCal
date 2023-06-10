#include <stdint.h>

#include "InkyCalInternal.h"

//These are increased with (the mock) logProblem below
uint64_t loggedWarnings = 0;
uint64_t loggedErrors   = 0;
uint64_t loggedFatals   = 0;

void logProblem(uint32_t severity)
{
    if (severity == INKY_SEVERITY_WARNING)
    {
        loggedWarnings++;
    }
    else if (severity == INKY_SEVERITY_ERROR)
    {
        loggedErrors++;
    }
    else if (severity == INKY_SEVERITY_FATAL)
    {
        loggedFatals++;
    }
    else
    {
        //TODO: add failed assert here
        //LogSerial_Error("logProblem called with unknown severity % " PRIu32 ".", severity);
        loggedErrors++;
    }
}