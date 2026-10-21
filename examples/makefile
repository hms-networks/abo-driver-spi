# SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
# Copyright (c) 2026 HMS Industrial Networks AB
#
# This file is part of the HMS Anybus ONE SDK and must only be used with
# HMS products. See the LICENSE file in the root of the SDK for full
# license terms.

# The example for the SPI host driver: one file, read top to bottom, and
# meant to be copied rather than run. The port in ports/template.c is empty,
# and the App_ functions are yours.
#
#   make            compile the example and the template port
#
# CC=gcc   the compiler; make's default is cc

EXAMPLES := io_channels

DRIVER_DIR := ..
BUILD_DIR  := build

# The generated header. The example uses its own sample here, for the pump in
# the README; point this at yours to build it against your configuration.
CONFIG_DIR ?= .

DRIVER_HEADERS := $(DRIVER_DIR)/inc/abo_spi.h

CFLAGS := -std=c99 -Wall -Wextra -pedantic -Werror
CFLAGS += -I$(DRIVER_DIR)/inc -I$(CONFIG_DIR) -Iports $(EXTRA_CFLAGS)

OBJECTS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(EXAMPLES) template))

HEADERS := $(DRIVER_HEADERS) ports/port.h $(CONFIG_DIR)/abo_spi_config.h

.PHONY: all clean

all: $(OBJECTS)
	@echo "== the example compiled"

$(BUILD_DIR)/%.o: %.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/template.o: ports/template.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
