# Overview

A Google Calendar Implementation for the InkPlate 6Color eInk display.

IMPORTANT: Before running the sketch - copy secrets.h.example to secrets.h and
set wifi password etc in the secrets.h version.

It was originally based on the example calendar sketch for the InkPlate 6 Color
but has been largely rewritten (aside from the display code). It has:

* Support for multiple calendars
* Support for calendars larger than RAM
* Support for all day events and (some) support for recurring events
* Support for filtering events

More details are in the [ChangeLog](ChangeLog.md)

Details of some unit tests can be found in the [test subdirectory](test)


## Dev Tips

* Can draw a calendar for a particular date by uncommenting example code in setup() in InkyCal.ino to
  (for example) debug weird behaviour on a particular date.