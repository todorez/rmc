# Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>

TOPDIR = $(shell if [ -z "$$PWD" ]; then pwd; else echo "$$PWD"; fi)

RMC_TOOL_SRC := $(wildcard src/*.c)
RMC_TOOL_OBJ := $(patsubst %.c,%.o,$(RMC_TOOL_SRC))

RMC_LIB_SRC := $(wildcard src/lib/common/*.c) src/lib/api.c
RMC_LIB_OBJ := $(patsubst %.c,%.o,$(RMC_LIB_SRC))

RMC_INSTALL_HEADERS := $(wildcard inc/*.h)

RMC_INSTALL_PREFIX := /usr

RMC_INSTALL_BIN_PATH := $(RMC_INSTALL_PREFIX)/bin/

RMC_INSTALL_LIB_PATH := $(RMC_INSTALL_PREFIX)/lib/

RMC_INSTALL_HEADER_PATH := $(RMC_INSTALL_PREFIX)/include/rmc/

ALL_OBJS := $(RMC_TOOL_OBJ) $(RMC_LIB_OBJ)

CFLAGS := -Wall -O2 -I$(TOPDIR)/inc $(RMC_CFLAGS)

all: rmc

$(ALL_OBJS): %.o: %.c
	@$(CC) -c $(CFLAGS) $< -o $@

librmc: $(RMC_LIB_OBJ)
	@$(AR) rcs src/lib/$@.a $^

rmc: $(RMC_TOOL_OBJ) librmc
	@$(CC) $(CFLAGS) -Lsrc/lib/ -lrmc $(RMC_TOOL_OBJ) src/lib/librmc.a -o src/$@

clean:
	@rm -f $(ALL_OBJS) src/rmc src/lib/librmc.a

.PHONY: clean rmc librmc

install:
	@mkdir -p $(RMC_INSTALL_BIN_PATH)
	@install -m 755 src/rmc $(RMC_INSTALL_BIN_PATH)
	@mkdir -p $(RMC_INSTALL_LIB_PATH)
	@install -m 644 src/lib/librmc.a $(RMC_INSTALL_LIB_PATH)
	@mkdir -p $(RMC_INSTALL_HEADER_PATH)
	@install -m 644 $(RMC_INSTALL_HEADERS) $(RMC_INSTALL_HEADER_PATH)
