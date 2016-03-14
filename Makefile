# Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>

RMC_TOOL_SRC := $(wildcard src/*.c)
RMC_TOOL_OBJ := $(patsubst %.c,%.o,$(RMC_TOOL_SRC))

RMCL_SRC :=$(wildcard src/rmcl/*.c)
RMCL_OBJ := $(patsubst %.c,%.o,$(RMCL_SRC))

RSMP_SRC :=$(wildcard src/rsmp/*.c)
RSMP_OBJ := $(patsubst %.c,%.o,$(RSMP_SRC))

# install rmcl library and necessary headers for rmcl.
RMC_INSTALL_HEADERS := inc/rmcl.h inc/rmc_types.h
RMC_INSTALL_LIBS := src/rmcl/librmcl.a

ifeq ($(strip $(RMC_INSTALL_PREFIX)),)
RMC_INSTALL_PREFIX := /usr
endif

RMC_INSTALL_HEADER_PATH := $(RMC_INSTALL_PREFIX)/include/
RMC_INSTALL_LIBS_PATH := $(RMC_INSTALL_PREFIX)/lib/

ALL_OBJS := $(RMC_TOOL_OBJ) $(RMCL_OBJ) $(RSMP_OBJ)

CFLAGS := -static -Wall -O2 -Iinc

all: rmc librmcl librsmp

$(ALL_OBJS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

librmcl: $(RMCL_OBJ)
	@$(AR) rcs src/rmcl/$@.a $<

librsmp: $(RSMP_OBJ)
	@$(AR) rcs src/rsmp/$@.a $<

rmc: $(RMC_TOOL_OBJ) librsmp librmcl
	$(CC) $(CFLAGS) -Lsrc/rsmp -Lsrc/rmcl -lrsmp -lrmcl $(RMC_TOOL_OBJ) src/rmcl/librmcl.a src/rsmp/librsmp.a -o src/$@

clean:
	@rm -f $(ALL_OBJS) src/rmc src/rmcl/librmcl.a src/rsmp/librsmp.a

.PHONY: clean rmc librmcl librsmp

install:
	@mkdir -p $(RMC_INSTALL_HEADER_PATH)
	@for each in $(RMC_INSTALL_HEADERS); do \
		install -m 644 $$each $(RMC_INSTALL_HEADER_PATH); done
	@mkdir -p $(RMC_INSTALL_LIBS_PATH)
	@for each in $(RMC_INSTALL_LIBS); do \
		install -m 644 $$each $(RMC_INSTALL_LIBS_PATH); done
