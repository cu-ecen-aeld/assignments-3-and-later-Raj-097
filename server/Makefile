# Makefile for aesdsocket program


# Target executable name
TARGET = aesdsocket

# Source files and Object files
SRCS = aesdsocket.c
OBJS = $(SRCS:%.c=%.o)

CC = aarch64-poky-linux-gcc

#CC = gcc
SYSROOT := $(wildcard $(HOME)/assignment-6/build/tmp/work/cortexa57-poky-linux/aesd-assignments/1.0+gitAUTOINC*/recipe-sysroot)



CFLAGS ?= $(TARGET_CC_ARCH)
CFLAGS += --sysroot=$(SYSROOT) -pthread -lrt

LDFLAGS ?= $(TARGET_LDFLAGS)
LDFLAGS += --sysroot=$(SYSROOT) -lgcc -lc


# Output directory
#OUT_DIR := ./server

# Default target
default: all

# Build target
all: $(TARGET)

# Compile the target binary
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	
# Rule to compile .c files into .o files in OUT_DIR
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET) *.o
	#rm -rf $(OUT_DIR)

# PHONY targets (not actual files)
.PHONY: all clean default
