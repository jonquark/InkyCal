* Calendar Data is sent sent Transfer-Encoding: chunked
     "After the headers are sent an http response will send the length of the chunk in hex, the chunk of data itself then a 0CRLF ("\r\n").
      https://en.wikipedia.org/wiki/Chunked_transfer_encoding"
  Improve the basic support that has been implemented, removing the hex digits at the start of chunks.

* Rework word wrap (example that works badly: "Spring Bank Holiday" breaks after Spring and before y)

* Add more advanced rule types:
             e.g.`{ LESS_THAN_AGO, "1 week", MOVE_EVENT, DAY2}`
      (some filters (like contains) will apply to all instances of an event or none -
         do we have installProcessList and eventprocesslist? I thinking no... run the same list twice
         (per event and per instance first time doing contains etc then second time does LESS_THAN_AGO))

* (Optional?) removal of dups between calendars

* fix more events that didn't parse and do something with timezone info we now parse from event DTSTART (maybe use timezone info in file)
       (Have both Mum and Dad events on 2024-04-07)

* unit test event processing esp in folded desc

* Document where event processing searches e.g. not in long summaries, locations

* maybe: add a logProblem counter for LogSerial_Unusual() (counter to be shown in normal operation?)