# Makefile for ProcessExplorerLite

# Detect OS
UNAME_S := $(shell uname -s)

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic
LDFLAGS = -lncurses

# Platform-specific static linking
ifeq ($(UNAME_S),Darwin)
    # macOS: Use Homebrew ncurses for static linking
    # Homebrew path differs between ARM and Intel Macs
    HOMEBREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /usr/local)
    NCURSES_PREFIX := $(HOMEBREW_PREFIX)/opt/ncurses
    STATIC_CFLAGS = -I$(NCURSES_PREFIX)/include
    # Force static linking by explicitly using the .a file
    STATIC_LDFLAGS = $(NCURSES_PREFIX)/lib/libncurses.a
else
    # Linux: Full static linking
    STATIC_CFLAGS =
    STATIC_LDFLAGS = -static -lncurses -ltinfo
endif

# Target executable
TARGET = processexplorer

# Source files
SRCS = main.c task_data.c
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete! Run with: ./$(TARGET)"

# Build static executable for release
static: clean
	$(CC) $(CFLAGS) $(STATIC_CFLAGS) -c $(SRCS)
	$(CC) $(OBJS) -o $(TARGET) $(STATIC_LDFLAGS)
	@echo "Static build complete! Run with: ./$(TARGET)"

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Clean complete!"

# Run the program
run: $(TARGET)
	./$(TARGET)

# Install (optional - requires sudo)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Help
help:
	@echo "ProcessExplorerLite Makefile"
	@echo "Available targets:"
	@echo "  make        - Build the program"
	@echo "  make static - Build static binary (for releases)"
	@echo "  make clean  - Remove build artifacts"
	@echo "  make run    - Build and run the program"
	@echo "  make install   - Install to /usr/local/bin (requires sudo)"
	@echo "  make uninstall - Remove from /usr/local/bin"

.PHONY: all clean run install uninstall help static
