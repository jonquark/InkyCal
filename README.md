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


*NOTE* Need to use 7.2.1 of the inkplate boards. If using a newer board the linker 
throws errors around overflowing dram (nb: boards are different to libraries)

## Debugging 

When replicating on a computer what data we receive can do:
echo 'GET /calendar/ical/en.uk%23holiday%40group.v.calendar.google.com/public/basic.ics HTTP/1.1
Host: calendar.google.com

' | openssl s_client -quiet -connect calendar.google.com:443 2>/dev/null >/tmp/openssl.caldata

