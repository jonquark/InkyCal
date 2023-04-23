* all day events (see:
   LogSerial_Error("Event with no date not time info (not yet handled): %.*s to %.*s", 8, dateStart, 8, dateEnd); 

* To map events to columns it compared "if (strncmp(day2, asctime(&event), 10) == 0)" - first 10 chars
  of asctime output don't include the year

* Define a structure that defines a google calendar including url, colour

* Get events from each calendar

* Add allowlist, denylist to calendar structure and update findEvents to do filtering

* Timezone handling (to account for daylight savings time)
  https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/

* Handling of calendars bigger than memory buffer (parsing in chunks)

* Change title drawn in drawInfo() - name of calendar won't work when multiple calendars
  (maybe time of last update + number of events (from each calendar?))


* sort out licensing

* Button - wake up example that would cause a refresh


