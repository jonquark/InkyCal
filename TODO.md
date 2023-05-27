
* Define a structure that defines a google calendar including url, defaultcolour

* Get events from each calendar

* Add ProcessorList to calendar structure and update code
  These are pointer to arrays of ProcessRule_t where the arrays are defined in secrets.h as well
  ProcessRules_t have the form { MatchType, MatchString, Result, ResultArg}
    so could be: `{ CONTAINS, "[nowall]", DISCARD, NULL}` 
             or  `{ DOES_NOT_CONTAIN, "[wall]", DISCARD, NULL}`
             or  `{ MATCHES_NOCASE_STRIP, "Dad", CHANGE_COLOUR, BLUE}` 
    (These initial rules can be processed per event (not instance) but see advanced rule TODO)

* Change title drawn in drawInfo() - name of calendar won't work when multiple calendars
  (maybe time of last update + number of events (from each calendar?) + number of errors)


* Timezone handling (to account for daylight savings time)
  https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/

 * Add more advanced rule types:
             e.g.`{ LESS_THAN_AGO, "1 week", MOVE_EVENT, DAY2}`
             or  `{ MATCHES_NOCASE_STRIP, "Dad", SET_SORT_TIE_PRIORITY, 10}`
      (some filters (like contains) will apply to all instances of an event or none -
         do we have installProcessList and eventprocesslist? I thinking no... run the same list twice
         (per event and per instance first time doing contains etc then second time does LESS_THAN_AGO)
    (For SET_SORT_TIE_PRIORITY, have cmp() use another extra new field (sortTiePriority) to split ties (all day events)))



* Currents reads network socket until no more data arrives for 10secs - but calendar is sent using
    Transfer-Encoding: chunked  (with one chunk - payload length sent in hex at start of body)
     "After the headers are sent an http response will send the length of the chunk in hex, the chunk of data itself then a 0CRLF ("\r\n").
      https://en.wikipedia.org/wiki/Chunked_transfer_encoding"

* Handling of calendars bigger than memory buffer (parsing in chunks)


* sort out licensing

* Button - wake up example that would cause a refresh


