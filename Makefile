PREFIX = /usr
MANPREFIX = $(PREFIX)/share/man
LIB = -lwayland-client -lsqlite3
EXE = cclipd
OBJ_COMMON = protocol/wlr-data-control-unstable-v1.o common.o db.o
CFLAGS = -Wall -Wpedantic

all: $(EXE)

cclipd: cclipd.o $(OBJ_COMMON)
	$(CC) cclipd.o $(OBJ_COMMON) $(LIB) -o $@

cclipd.o: cclipd.c common.h db.h protocol/wlr-data-control-unstable-v1-client-protocol.h

protocol/wlr-data-control-unstable-v1.c: protocol/wlr-data-control-unstable-v1.xml
	wayland-scanner private-code protocol/wlr-data-control-unstable-v1.xml $@

protocol/wlr-data-control-unstable-v1-client-protocol.h: protocol/wlr-data-control-unstable-v1.xml
	wayland-scanner client-header protocol/wlr-data-control-unstable-v1.xml $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(EXE) protocol/*.[och]

