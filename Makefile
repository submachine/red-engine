RED_BINARY = red-engine
RED_SOURCE = red-engine.c red-daemon.c
RED_HEADER = red-engine.h

CFLAGS += -Wall -O2 -std=c99 -pedantic
CFLAGS += $(shell pkg-config --cflags libmicrohttpd glib-2.0)
LIBS   += $(shell pkg-config --libs libmicrohttpd glib-2.0)
LIBS   += -ldb

.PHONY: all clean install uninstall

all: $(RED_BINARY)

$(RED_BINARY): $(RED_SOURCE) $(RED_HEADER)
	$(CC) $(CFLAGS) $(RED_SOURCE) $(LIBS) -o $@

clean:
	rm -f $(RED_BINARY)

PREFIX = /usr/local

install: all
	test -d $(PREFIX) || mkdir $(PREFIX)
	test -d $(PREFIX)/bin || mkdir $(PREFIX)/bin
	install -m 0755 $(RED_BINARY) $(PREFIX)/bin

uninstall:
	test -e $(PREFIX)/bin/$(RED_BINARY) \
	&& rm $(PREFIX)/bin/$(RED_BINARY)
