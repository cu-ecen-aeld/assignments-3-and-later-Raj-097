# Set default values for CC and LD (native compiler)
CC = gcc


# Compiler flags
CFLAGS = -Wall -g
#CFLAGS += -I/usr/include -I/usr/local/include

# Target to build the "writer" application
TARGET = writer

# Object files
OBJECTS = writer.o

# Source files
SOURCES = writer.c

# Cross-compilation setup
ifeq ($(CROSS_COMPILE),)
	# If no cross-compiler specified, use the native compiler
	CC = gcc
	LD = ld
else
	# Cross-compilation setup: use the specified cross-compiler prefix
	CC = $(CROSS_COMPILE)gcc
	LD = $(CROSS_COMPILE)ld
endif

#default target to build the application
all: $(TARGET)

# Rule to build the target executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

# Rule to compile the C source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target to remove compiled files
clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean


