* Move code that parses events out of http data from drawInfo/drawData into new
  findEvents() function or similar

* Log to serial events - have a log routine with DEBUG() DEBBUG_MORE() and DEBUG_MAX() 

* Change title drawn in drawInfo() - name of calendar won't work when multiple calendars
  (maybe time of last update + number of events (from each calendar?))

* Define a structure that defines a google calendar including url, colour

* Get events from each calendar

* Add allowlist, denylist to calendar structure and update findEvents to do filtering

* sort out licensing

* Button - wake up example that would cause a refresh


