CC=gcc
CFLAGS=-O3 -prebind -pedantic -Wmost
LDFLAGS=-framework Carbon
CPPFLAGS=-no-cpp-precomp
prefix=/usr/local
destdir=

prog=lsmac
$(prog): lsmac.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) lsmac.c -o $@

clean:
	-rm $(prog) core*

install: $(prog)
	mkdir -p $(destdir)$(prefix)/bin
	install -m 755 $(prog) $(destdir)$(prefix)/bin

uninstall:
	-rm $(destdir)$(prefix)/bin/$(prog)
