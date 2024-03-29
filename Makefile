RED_BINARY = red-engine
RED_SOURCE = red-engine.c red-daemon.c
RED_HEADER = red-engine.h

RED_CHECK = check/red-engine.sh
RED_TEST_HOME = test

CFLAGS += -Wall -O2 -std=c99 -pedantic
CFLAGS += $(shell pkg-config --cflags libmicrohttpd)
LIBS   += $(shell pkg-config --libs libmicrohttpd)
LIBS   += -ldb

.PHONY: all check clean install uninstall

all: $(RED_BINARY)

$(RED_BINARY): $(RED_SOURCE) $(RED_HEADER)
	$(CC) $(CFLAGS) $(RED_SOURCE) $(LIBS) -o $@

check: all
	TEST_HOME=$(RED_TEST_HOME) TEST_BINARY=./$(RED_BINARY) $(RED_CHECK)

clean:
	rm -f $(RED_BINARY)
	rm -rf $(RED_TEST_HOME)

PREFIX = /usr/local

install: all
	test -d $(PREFIX) || mkdir $(PREFIX)
	test -d $(PREFIX)/bin || mkdir $(PREFIX)/bin
	install -m 0755 $(RED_BINARY) $(PREFIX)/bin

uninstall:
	test -e $(PREFIX)/bin/$(RED_BINARY) \
	&& rm $(PREFIX)/bin/$(RED_BINARY)
