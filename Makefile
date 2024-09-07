CFLAGS = -Wall -Wextra -Wpedantic -Wno-unused-parameter -Og -g

all: cclipd

cclipd: cclipd.o db.o common.o wayland.o pending_offers.o protocol/wlr-data-control-unstable-v1.o
	$(CC) $^ -lwayland-client -lsqlite3 -o $@

cclipd.o: cclipd.c protocol/wlr-data-control-unstable-v1-client-protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

db.o: db.c db.h common.h
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

clean:
	rm -vf *.o cclipd protocol/*.[och]

.PHONY: all clean
