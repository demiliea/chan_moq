# Makefile for chan_moq.so
# Asterisk Media over QUIC Channel Driver

CC=gcc
CFLAGS=-Wall -Wextra -fPIC -D_GNU_SOURCE -O2
LDFLAGS=-shared
LIBS=-lpthread -ljson-c -lwebsockets

# Asterisk directories
ASTERISK_INCLUDE=/usr/include/asterisk
ASTERISK_MODULES=/usr/lib/asterisk/modules

# Check for alternative Asterisk paths
ifeq ($(wildcard $(ASTERISK_INCLUDE)),)
    ASTERISK_INCLUDE=/usr/local/include/asterisk
endif

# Source files
SOURCES=chan_moq.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=chan_moq.so

# Build rules
all: check-deps $(TARGET)

check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists libwebsockets || echo "WARNING: libwebsockets not found. Install: sudo apt-get install libwebsockets-dev"
	@pkg-config --exists json-c || echo "WARNING: json-c not found. Install: sudo apt-get install libjson-c-dev"
	@test -d $(ASTERISK_INCLUDE) || echo "WARNING: Asterisk headers not found at $(ASTERISK_INCLUDE)"
	@echo "Dependency check complete (proceeding with build)"

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo ""
	@echo "Build successful!"
	@echo "To install: sudo make install"
	@echo ""

%.o: %.c
	$(CC) $(CFLAGS) -I$(ASTERISK_INCLUDE) -c $< -o $@

install: $(TARGET)
	@echo "Installing chan_moq.so..."
	@if [ ! -d $(ASTERISK_MODULES) ]; then \
		echo "Creating modules directory: $(ASTERISK_MODULES)"; \
		mkdir -p $(ASTERISK_MODULES); \
	fi
	install -m 755 $(TARGET) $(ASTERISK_MODULES)/
	@if [ -f moq.conf ]; then \
		if [ ! -f /etc/asterisk/moq.conf ]; then \
			echo "Installing moq.conf..."; \
			install -m 644 moq.conf /etc/asterisk/; \
		else \
			echo "moq.conf already exists, skipping"; \
		fi \
	fi
	@echo ""
	@echo "Installation complete!"
	@echo "Load module with: asterisk -rx \"module load chan_moq.so\""
	@echo ""

uninstall:
	rm -f $(ASTERISK_MODULES)/$(TARGET)
	@echo "Module uninstalled"

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "Clean complete"

test: $(TARGET)
	@echo "Testing module dependencies..."
	ldd $(TARGET)

reload: all
	@echo "Reloading chan_moq.so..."
	@asterisk -rx "module unload chan_moq.so" 2>/dev/null || true
	@$(MAKE) install
	@asterisk -rx "module load chan_moq.so"

debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all

help:
	@echo "MoQ Channel Driver - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build chan_moq.so (default)"
	@echo "  install    - Install module to Asterisk"
	@echo "  uninstall  - Remove module from Asterisk"
	@echo "  clean      - Remove build artifacts"
	@echo "  test       - Test module dependencies"
	@echo "  reload     - Build, install, and reload in Asterisk"
	@echo "  debug      - Build with debug symbols"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Example usage:"
	@echo "  make                  # Build"
	@echo "  sudo make install     # Install"
	@echo "  make clean            # Clean build files"
	@echo ""

.PHONY: all check-deps install uninstall clean test reload debug help
