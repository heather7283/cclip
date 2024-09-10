CC = clang
CFLAGS = -Wall -Wextra -Wpedantic -Wno-unused-parameter -Og -g

VERSION != ver="$$(git describe --long)"; [ -n "$$ver" ] && printf "$$ver" | sed 's/\([^-]*-g\)/r\1/;s/-/./g' || printf 0.0.0

all: cclipd cclip

cclipd: cclipd.o db.o wayland.o pending_offers.o config.o protocol/wlr-data-control-unstable-v1.o
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

pending_offers.o: pending_offers.c pending_offers.h common.h
	$(CC) $(CFLAGS) -c $< -o $@

protocol/wlr-data-control-unstable-v1.o: protocol/wlr-data-control-unstable-v1.c
	$(CC) $(CFLAGS) -c $< -o $@

protocol/wlr-data-control-unstable-v1.c: protocol/wlr-data-control-unstable-v1.xml
	wayland-scanner private-code $< $@

protocol/wlr-data-control-unstable-v1-client-protocol.h: protocol/wlr-data-control-unstable-v1.xml
	wayland-scanner client-header $< $@

clean:
	rm -vf *.o cclipd protocol/*.[och]

.PHONY: all clean
