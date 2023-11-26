* Currents reads network socket until no more data arrives for 10secs - but calendar is sent using
    Transfer-Encoding: chunked  (with one chunk - payload length sent in hex at start of body)
     "After the headers are sent an http response will send the length of the chunk in hex, the chunk of data itself then a 0CRLF ("\r\n").
      https://en.wikipedia.org/wiki/Chunked_transfer_encoding"

* Rework word wrap (example that works badly: "Spring Bank Holiday" breaks after Spring and before y)

* Add more advanced rule types:
             e.g.`{ LESS_THAN_AGO, "1 week", MOVE_EVENT, DAY2}`
      (some filters (like contains) will apply to all instances of an event or none -
         do we have instanceProcessList and eventprocesslist? I thinking no... run the same list twice
         (per event and per instance first time doing contains etc then second time does LESS_THAN_AGO))

* (Optional?) removal of dups between calendars

* Currently works reliably with Inkplate Library 7.0.0 (easy to change version in "Library Manager" - investigate crashes during drawing with 8.1.0)

* fix more events that didn't parse and do something with timezone info we now parse from event DTSTART (maybe use timezone info in file)

* unit test event processing esp in folded desc

* Document where event processing searches e.g. not in long summaries, locations

* maybe: add a logProblem counter for LogSerial_Unusual() (counter to be shown in normal operation?)

* Duplicate mum events on Aug-31-2023:
     Store uids and sequence with events
     
     If event has recurrence-id then:
         from the recurrence-id - see if the old event would have been relevant,
            if so, compare sequence and if new sequence >= then  go through and remove relevant event day(s)
