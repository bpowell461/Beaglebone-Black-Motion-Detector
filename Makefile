# Compiler and linker configuration
CC := gcc
LDFLAGS := -lrt -pthread
INCLUDES := $(shell find . -type f -name '*.h' -exec dirname {} \; | sort -u | sed 's/^/-I/')
CFLAGS := -O2 -march=armv7-a -mtune=cortex-a8 -mfpu=neon -ftree-vectorize -ffast-math -fno-omit-frame-pointer -std=gnu99 -fexceptions -DBEAGLEBONE -DDEBUG -DLOG_AND_PRINT

# Target executable name
TARGET := detector

# Find all .c files in the subdirectory
SRCS := $(shell find $(SOURCEDIR) -name '*.c')

# Object files to be created from the .c files
OBJS := $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Link the object files into the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Rule to compile each .c file to an .o file
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean target to remove build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
