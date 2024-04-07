This started out based on the Google Calendar InkPlate example
but there are lots of changes:

Features:

* Can get events from multiple calendars
* Can set rules to filter events or e.g. change the colour used
* Lots more (configurably) diagnostic logging

Fixes:

* The example happily read more calendar data than fitted in the buffer (of memory)

* The example checked whether the field of events were null - but the expression
  that set them added offsets (like +5) so it never correctly detected when field werent set

* When reading calendar data down the https connection, if a character wasn't available
  it immediately decided the stream had ended - often resulting in incomplete calendar reads

* To map events to columns it compared "if (strncmp(day2, asctime(&event), 10) == 0)" - first 10 chars
  of asctime output don't include the year

* Timezone handling fixed to work across DST boundaries
 