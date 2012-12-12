CC=gcc
CFLAGS=-O3 -prebind
LDFLAGS=-framework Carbon
CPPFLAGS=-no-cpp-precomp
prefix=/usr/local
destdir=

prog=lsmac
$(prog):
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) lsmac.c -o $@

clean:
	-rm $(prog) core*

install: $(prog)
	mkdir -p $(destdir)$(prefix)/bin
	install -m 755 $(prog) $(destdir)$(prefix)/bin

uninstall:
	-rm $(destdir)$(prefix)/bin/$(prog)
