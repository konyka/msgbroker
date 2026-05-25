CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -O2 -g
PREFIX  ?= /usr/local
AR      ?= ar
RANLIB  ?= ranlib

MB_MAJOR = 0
MB_MINOR = 1
MB_PATCH = 0

SOURCES  = $(shell find src -name '*.c')
OBJECTS  = $(SOURCES:.c=.o)
HEADERS  = $(shell find include -name '*.h')

LIB_NAME    = msgbroker
LIB_SHARED  = lib$(LIB_NAME).so
LIB_STATIC  = lib$(LIB_NAME).a
LIB_VERSION = $(LIB_SHARED).$(MB_MAJOR).$(MB_MINOR).$(MB_PATCH)

CFLAGS += -Iinclude -I.

.PHONY: all shared static clean install test examples perf

all: shared static

shared: $(LIB_SHARED)

static: $(LIB_STATIC)

$(LIB_SHARED): $(OBJECTS)
	$(CC) -shared -o $@ $^ -lpthread $(LDFLAGS) -Wl,-soname,$(LIB_SHARED).$(MB_MAJOR)
	ln -sf $@ $(LIB_SHARED).$(MB_MAJOR)

$(LIB_STATIC): $(OBJECTS)
	$(AR) rcs $@ $^
	$(RANLIB) $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(LIB_SHARED) $(LIB_SHARED).* $(LIB_STATIC)
	find tests examples perf -name '*.o' -delete 2>/dev/null || true

install: all
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include/msgbroker
	install -m 644 $(LIB_STATIC) $(DESTDIR)$(PREFIX)/lib/
	install -m 755 $(LIB_SHARED) $(DESTDIR)$(PREFIX)/lib/
	cp -r include/msgbroker/*.h $(DESTDIR)$(PREFIX)/include/msgbroker/

test: all
	@echo "Tests not yet configured. Use cmake build for tests."

examples: all
	@echo "Examples not yet configured."

perf: all
	@echo "Perf targets not yet configured."
