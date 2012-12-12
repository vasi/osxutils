NAMES_CARBON = fileinfo getfcomment hfsdata lsmac mkalias setfcomment setfctypes setfflags setlabel setsuffix
NAMES_COCOA = geticon seticon wsupdate

PROGRAMS_CARBON = $(foreach name,$(NAMES_CARBON),$(name)/$(name))
PROGRAMS_COCOA = $(foreach name,$(NAMES_COCOA),$(name)/$(name))

PROGRAMS = $(PROGRAMS_COCOA) $(PROGRAMS_CARBON)

CC = gcc
OPT = -O0 -g
MY_CFLAGS = -fpascal-strings
WARN = -w

all: $(PROGRAMS)

clean:
	find . -name '*.o' -exec rm {} \+
	rm -f $(PROGRAMS)

.PHONY: all clean


%.o: %.c
	$(CC) $(OPT) $(WARN) $(MY_CFLAGS) $(CFLAGS) -c -o $@ $<
%.o: %.m
	$(CC) $(OPT) $(WARN) $(MY_CFLAGS) $(CFLAGS) -c -o $@ $<


OBJFILES = $(patsubst %.c,%.o,$(patsubst %.m,%.o,$(wildcard $(1)/*.[cm])))

define TEMPLATE
$(1)/$(1): $(call OBJFILES,$(1))
	$(CC) $(OPT) $(WARN) -o $$@ -framework $(2) $$^
endef

$(foreach name,$(NAMES_CARBON),$(eval $(call TEMPLATE,$(name),Carbon)))
$(foreach name,$(NAMES_COCOA),$(eval $(call TEMPLATE,$(name),Cocoa)))
