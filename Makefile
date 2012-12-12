OPT = -O0 -g
ARCH =
MY_CFLAGS = -fpascal-strings
WARN = -w


NAMES_CARBON = fileinfo getfcomment hfsdata lsmac mkalias setfcomment setfctypes setfflags setlabel setsuffix
NAMES_COCOA = geticon seticon wsupdate
NAMES_SCRIPT = cpath google osxutils rcmac setvolume trash wiki
NAMES = $(NAMES_CARBON) $(NAMES_COCOA)
PROGRAMS = $(foreach name,$(NAMES),$(name)/$(name))
SCRIPTS = $(foreach name,$(NAMES_SCRIPT),$(name)/$(name))
MANPAGES = $(wildcard */*.1)


all: $(NAMES)

clean:
	find . -name '*.o' -exec rm {} \+
	rm -f $(PROGRAMS)

.PHONY: all clean install install install-man install-bin $(NAMES)


PREFIX=$(DESTDIR)/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1

install-bin: $(PROGRAMS) $(SCRIPTS)
	install -d -m 755 $(BINDIR)
	install -m 755 $(PROGRAMS) $(SCRIPTS) $(BINDIR)

install-man: $(MANPAGES)
	install -d -m 755 $(MANDIR)
	install -m 644 $(MANPAGES) $(MANDIR)

install: install-bin install-man


ARCH_FLAG = $(if $(ARCH),-arch $(ARCH),)
COMPILER = $(CC) $(ARCH_FLAG) $(OPT) $(WARN)
COMPILE = $(COMPILER) $(MY_CFLAGS) $(CFLAGS)

%.o: %.c
	$(COMPILE) -c -o $@ $<
%.o: %.m
	$(COMPILE) -c -o $@ $<

F_OBJFILES = $(patsubst %.c,%.o,$(patsubst %.m,%.o,$(wildcard $(1)/*.[cm])))

define TEMPL_CC
$(1)/$(1): $(call F_OBJFILES,$(1))
	$(COMPILER) $(LDFLAGS) -o $$@ -framework $(2) $$^
endef

$(foreach name,$(NAMES_CARBON),$(eval $(call TEMPL_CC,$(name),Carbon)))
$(foreach name,$(NAMES_COCOA),$(eval $(call TEMPL_CC,$(name),Cocoa)))
$(foreach name,$(NAMES),$(eval $(name): $(name)/$(name)))
