
CC=g++

IFLAGS = -I. -I$(PRJSRC)

define eyecatcher
	@echo ==== $(1) ==== $(shell date +%T)
endef

define ensure-output-dir
	@mkdir -p $(dir $(1))
endef

define buildrecipe-basic-unittest
	$(call eyecatcher, Build Unit Test:${notdir $@})
	$(call ensure-output-dir, $@)
	$(CC) -g -ggdb -O0 -o $@ $(IFLAGS) $^
endef

define  exec-basic-unittest
	$(call eyecatcher, Run Test Target:$(notdir $@) Binary:$(notdir $(1)))
	$(1) $(2)
endef

#Function: Build a unit test and add it to the list of tests to run
#Parameter 1: Name of test executable
#Parameter 2: (space separated) source files
define build-basic-unittest
TEST-TARGETS += $(BINDIR)/$(strip $(1))

$(BINDIR)/$(strip $(1)): $2
	$$(call buildrecipe-basic-unittest)

EXEC-TEST-TARGETS += exec_$(strip $(1))

exec_$(strip $(1)): $(BINDIR)/$(strip $(1))
	$$(call exec-basic-unittest, $$<)

.PHONY:: exec_$(strip $(1))
endef