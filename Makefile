RED_BINARY = red-engine
RED_SOURCE = red-engine.c red-daemon.c
RED_HEADER = red-engine.h

CFLAGS += $(shell pkg-config --cflags libmicrohttpd glib-2.0)
LIBS   += $(shell pkg-config --libs libmicrohttpd glib-2.0)

all: $(RED_BINARY)

$(RED_BINARY): $(RED_SOURCE) $(RED_HEADER)
	$(CC) $(CFLAGS) $(RED_SOURCE) $(LIBS) -o $@

clean:
	@rm -rf $(RED_BINARY)
