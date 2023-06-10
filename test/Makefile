PRJROOT=..
PRJSRC=$(PRJROOT)
TESTROOT=.
MOCKSRC=$(TESTROOT)/mock
UTILSSRC=$(TESTROOT)/utils
BINDIR=$(TESTROOT)/bin

include $(TESTROOT)/makefuncs.mk

#$(info $(call build-basic-unittest, testCal,  $(TESTROOT)/testCalendar.c))
$(eval $(call build-basic-unittest, testCal, \
                                 $(TESTROOT)/testCalendar.c \
								 $(UTILSSRC)/test_utils_filetostring.c \
								 $(PRJSRC)/Calendar.cpp \
								 $(PRJSRC)/entry.cpp \
								 $(PRJSRC)/EventProcessing.cpp \
								 $(MOCKSRC)/mockLogSerial.cpp \
								 $(MOCKSRC)/mockLogProblem.cpp))

buildtests: $(TEST-TARGETS) 

test: $(EXEC-TEST-TARGETS)

clean:
	rm -rf $(BINDIR)

.PHONY:: buildtests test clean

# The default goal (target) when none is specified
.DEFAULT_GOAL=buildtests