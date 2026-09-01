CC ?= gcc
NASM ?= nasm

CFLAGS ?= -O2 -Wall -Wextra -std=c11 -pthread
LDFLAGS ?= -pthread
LDLIBS ?= -lgmp

BUILD_DIR := build
TARGET := picalc

ASM_OBJS := $(BUILD_DIR)/main.o
C_OBJS := $(BUILD_DIR)/chudnovsky.o
OBJS := $(ASM_OBJS) $(C_OBJS)
NATIVE_BIGINT_TEST := $(BUILD_DIR)/native_bigint_test

.PHONY: all clean test

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/main.o: src/main.asm | $(BUILD_DIR)
	$(NASM) -f elf64 -g -F dwarf $< -o $@

$(BUILD_DIR)/chudnovsky.o: src/chudnovsky.c src/chudnovsky.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

test: $(TARGET) $(NATIVE_BIGINT_TEST)
	./tests/smoke.sh
	$(NATIVE_BIGINT_TEST)

$(NATIVE_BIGINT_TEST): tests/native_bigint_test.c src/native_bigint.c src/native_bigint.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/native_bigint_test.c src/native_bigint.c -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
