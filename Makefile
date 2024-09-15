CFLAGS = -Wall -Wextra -Wpedantic -Og -g
# both gcc and clang support it, and it makes my life easier
CFLAGS += -Wno-gnu-zero-variadic-macro-arguments

VERSION != ver="$$(git describe --long)"; [ -n "$$ver" ] && printf "$$ver" | sed 's/\([^-]*-g\)/r\1/;s/-/./g' || printf 0.0.0

all: cclipd cclip

cclipd: cclipd.o db.o wayland.o config.o protocol/wlr-data-control-unstable-v1.o
	$(CC) $(LDFLAGS) $^ -lwayland-client -lsqlite3 -o $@

cclipd.o: cclipd.c protocol/wlr-data-control-unstable-v1-client-protocol.h common.h
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c $< -o $@

cclip: cclip.o db.o config.o
	$(CC) $(LDFLAGS) $^ -lsqlite3 -o $@

cclip.o: cclip.c
	$(CC) $(CFLAGS) -c $< -o $@

db.o: db.c db.h common.h
	$(CC) $(CFLAGS) -c $< -o $@

config.o: config.c config.h common.h
	$(CC) $(CFLAGS) -c $< -o $@

wayland.o: wayland.c wayland.h common.h
	$(CC) $(CFLAGS) -c $< -o $@

protocol/wlr-data-control-unstable-v1.o: protocol/wlr-data-control-unstable-v1.c
	$(CC) $(CFLAGS) -c $< -o $@

protocol/wlr-data-control-unstable-v1.c: protocol/wlr-data-control-unstable-v1.xml
	wayland-scanner private-code $< $@

protocol/wlr-data-control-unstable-v1-client-protocol.h: protocol/wlr-data-control-unstable-v1.xml
	wayland-scanner client-header $< $@

clean:
	rm -vf *.o cclipd cclip protocol/*.[och]

.PHONY: all clean
