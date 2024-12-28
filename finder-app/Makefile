# Set default values for CC and LD (native compiler)
CC = gcc

# Compiler flags
CFLAGS = -Wall -g

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


#_______Application________
all: $(TARGET)

# Rule to build the target executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

# Rule to compile the C source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


# Clean target to remove compiled app
clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: app clean 

