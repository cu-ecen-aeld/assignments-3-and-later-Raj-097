# Makefile for aesdsocket program


# Target executable name
TARGET = aesdsocket

# Source files and Object files
SRCS = aesdsocket.c
OBJS = $(SRCS:%.c=%.o)

# Compiler settings
CC ?= $(TARGET_CC)
LDFLAGS += $(TARGET_LDFLAGS)

CFLAGS = -Wall -Wextra -g


# Output directory
#OUT_DIR := ./server

# Default target
default: all

# Build target
#all: $(OUT_DIR)/$(TARGET)
all: $(TARGET)

# Compile the target binary
#$(OUT_DIR)/$(TARGET): $(OBJS) | $(OUT_DIR)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Rule to compile .c files into .o files in OUT_DIR
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create the output directory if it does not exist
#$(OUT_DIR):
#	mkdir -p $(OUT_DIR)

# Clean up build artifacts
clean:
	rm -f $(TARGET) *.o
	#rm -rf $(OUT_DIR)

# PHONY targets (not actual files)
.PHONY: all clean default
