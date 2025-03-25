# Makefile for aesdsocket program


# Target executable name
TARGET = aesdsocket

# Source files and Object files
SRCS = aesdsocket.c
OBJS = $(SRCS:%.c=%.o)

# Compiler settings
CC ?= aarch64-poky-linux-gcc
# Use the correct compiler flags from Yocto
CFLAGS += -Wall -Wextra -g $(TARGET_CC_ARCH)
LDFLAGS += $(TARGET_LDFLAGS)


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
