# Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>

RMC_TOOL_SRC := $(wildcard src/*.c)
RMC_TOOL_OBJ := $(patsubst %.c,%.o,$(RMC_TOOL_SRC))

RMCL_SRC :=$(wildcard src/rmcl/*.c)
RMCL_OBJ := $(patsubst %.c,%.o,$(RMCL_SRC))

RSMP_SRC :=$(wildcard src/rsmp/*.c)
RSMP_OBJ := $(patsubst %.c,%.o,$(RSMP_SRC))

ALL_OBJS := $(RMC_TOOL_OBJ) $(RMCL_OBJ) $(RSMP_OBJ)

CFLAGS := -static -Wall -O2 -Iinc

all: rmc librmcl librsmp

$(ALL_OBJS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

librmcl: $(RMCL_OBJ)
	@ar rcs src/rmcl/$@.a $<

librsmp: $(RSMP_OBJ)
	@ar rcs src/rsmp/$@.a $<

rmc: $(RMC_TOOL_OBJ) librsmp librmcl
	$(CC) $(CFLAGS) -Lsrc/rsmp -Lsrc/rmcl -lrsmp -lrmcl $(RMC_TOOL_OBJ) src/rmcl/librmcl.a src/rsmp/librsmp.a -o src/$@

clean:
	@rm -f $(ALL_OBJS) src/rmc src/rmcl/librmcl.a src/rsmp/librsmp.a

.PHONY: clean rmc librmcl librsmp

