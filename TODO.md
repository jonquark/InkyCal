* all day events not tested



* Define a structure that defines a google calendar including url, colour

* Get events from each calendar

* Add allowlist, denylist to calendar structure and update findEvents to do filtering

* Timezone handling (to account for daylight savings time)
  https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/

* Currents reads network socket until no more data arrives for 10secs - but calendar is sent using
    Transfer-Encoding: chunked  (with one chunk - payload length sent in hex at start of body)
     "After the headers are sent an http response will send the length of the chunk in hex, the chunk of data itself then a 0CRLF ("\r\n").
      https://en.wikipedia.org/wiki/Chunked_transfer_encoding"

* Handling of calendars bigger than memory buffer (parsing in chunks)

* Change title drawn in drawInfo() - name of calendar won't work when multiple calendars
  (maybe time of last update + number of events (from each calendar?))


* sort out licensing

* Button - wake up example that would cause a refresh


