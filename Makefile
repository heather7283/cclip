CC = clang
CFLAGS = -Wall -Wextra -Wpedantic -Wno-unused-parameter -Og -g

INIH_FLAGS = -DINI_ALLOW_MULTILINE=0 -DINI_STOP_ON_FIRST_ERROR=1

all: cclipd

cclipd: cclipd.o db.o common.o wayland.o pending_offers.o config.o protocol/wlr-data-control-unstable-v1.o
	$(CC) $(LDFLAGS) $^ -lwayland-client -lsqlite3 -o $@

cclipd.o: cclipd.c protocol/wlr-data-control-unstable-v1-client-protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

db.o: db.c db.h common.h
	$(CC) $(CFLAGS) -c $< -o $@

config.o: config.c config.h common.h
	$(CC) $(CFLAGS) -c $< -o $@

common.o: common.c common.h
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

inih/ini.o: inih/ini.c inih/ini.h
	$(CC) $(CFLAGS) $(INIH_FLAGS) -c $< -o $@

clean:
	rm -vf *.o cclipd protocol/*.[och] inih/ini.o

.PHONY: all clean
