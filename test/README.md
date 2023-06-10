# Testing Overview

In this directory as some simple unit tests that are *not* meant to run on the InkPlate. 
Currently they only cover the calendar parsing code as that is the least-embedded specific 
code.

(I run the tests on my Linux dev box).

To run the tests:
```
cd tests #the tests dir needs to be the current directory
make test
```
Or to build the tests without run them (output does into the bin subdirectory) just
run `make`