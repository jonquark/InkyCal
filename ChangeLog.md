This started out based on the Google Calendar InkPlate example
but there are lots of changes:

Features:

* Lots more (configurably) diagnostic logging
* Can set rules to filter events or e.g. change the colour used
* Can get events from multiple calendars


Fixes:

* The example happily read more calendar data than fitted in the buffer (of memory)

* The example checked whether the field of events were null - but the expression
  that set them added offsets (like +5) so it never correctly detected when field werent set

* When reading calendar data down the https connection, if a character wasn't available
  it immediately decided the stream had ended - often resulting in incomplete calendar reads
  (this code probably waits too long for more data now - revisit - using chunk length at start of payload)

* To map events to columns it compared "if (strncmp(day2, asctime(&event), 10) == 0)" - first 10 chars
  of asctime output don't include the year
 