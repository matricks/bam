# install directories
INSTALL_PREFIX = /usr
INSTALL_BINDIR = $(INSTALL_PREFIX)/bin


# toolchain
PKG_CONFIG ?= pkg-config
PYTHON ?= python


# flags
LUA_LIBS := $(shell $(PKG_CONFIG) --libs lua 2>/dev/null || echo "-llua")
LIBS += -lm -lpthread $(LUA_LIBS) -ldl
LUA_CFLAGS := $(shell $(PKG_CONFIG) --cflags lua 2>/dev/null || echo "-I/usr/include/lua")
CFLAGS += $(LUA_CFLAGS)


# objects
TARGETS = txt2c internal_base bam
BAM_OBJ = $(patsubst %.c,%.o,$(wildcard src/*.c))
TXT2C_LUA = $(wildcard src/*.lua)


# make rules
all: $(TARGETS)

txt2c: src/tools/txt2c

internal_base: src/internal_base.h

src/internal_base.h:
	src/tools/txt2c $(TXT2C_LUA) > src/internal_base.h

bam: txt2c internal_base $(BAM_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(BAM_OBJ) $(LIBS)

test: $(TARGETS)
	$(PYTHON) scripts/test.py

install: bam
	install -d "$(DESTDIR)$(INSTALL_BINDIR)"
	install -m755 bam "$(DESTDIR)$(INSTALL_BINDIR)"/bam

clean:
	rm -f $(BAM_OBJ) $(TARGETS) src/internal_base.h src/tools/txt2c


.PHONY: all test install clean

