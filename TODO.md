* Change title drawn in drawInfo() - name of calendar won't work when multiple calendars
  (maybe time of last update + number of events (from each calendar?) + number of errors)

* Add eventdesc (multi-line so hard to determine end to eventprocessing)

* Timezone handling (to account for daylight savings time)
  https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/

* Currents reads network socket until no more data arrives for 10secs - but calendar is sent using
    Transfer-Encoding: chunked  (with one chunk - payload length sent in hex at start of body)
     "After the headers are sent an http response will send the length of the chunk in hex, the chunk of data itself then a 0CRLF ("\r\n").
      https://en.wikipedia.org/wiki/Chunked_transfer_encoding"

* sort out licensing

* Button - wake up example that would cause a refresh

* Add more advanced rule types:
             e.g.`{ LESS_THAN_AGO, "1 week", MOVE_EVENT, DAY2}`
             or  `{ MATCHES_NOCASE_STRIP, "Dad", SET_SORT_TIE_PRIORITY, 10}`
      (some filters (like contains) will apply to all instances of an event or none -
         do we have installProcessList and eventprocesslist? I thinking no... run the same list twice
         (per event and per instance first time doing contains etc then second time does LESS_THAN_AGO)
    (For SET_SORT_TIE_PRIORITY, have cmp() use another extra new field (sortTiePriority) to split ties (all day events)))

* %ld is not right for 64bit types %" PRIu64 ¨ or %" PRId64 ¨
