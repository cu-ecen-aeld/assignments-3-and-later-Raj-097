# Makefile for aesdsocket program

# Compiler settings
CC = gcc
#CFLAGS = -Wall -Wextra -g

# Target executable name
TARGET = aesdsocket

# Source files and Object files
SRCS = aesdsocket.c
OBJS = $(SRCS:%.c=%.o)

# Compiler settings
#CC ?= $(TARGET_CC)

#LD ?= aarch64-poky-linux-ld  
CFLAGS = -Wall -Wextra -g
#LDFLAGS += $(TARGET_LDFLAGS)
 

# Cross-compilation setup
#ifeq ($(CROSS_COMPILE),)
	# If no cross-compiler specified, use the native compiler
#	CC = gcc
#	LD = ld
#else
	# Cross-compilation setup: use the specified cross-compiler prefix
#	CC = $(CROSS_COMPILE)gcc
#	LD = $(CROSS_COMPILE)ld
#endif

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
	$(CC) $(CFLAGS) -o $@ $^  #$(LDFLAGS)

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

