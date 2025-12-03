# Makefile for chan_moq.so
# Asterisk Media over QUIC Channel Driver

CC=gcc
CFLAGS=-Wall -Wextra -fPIC -D_GNU_SOURCE
LDFLAGS=-shared
LIBS=-lpthread

# Asterisk directories
ASTERISK_INCLUDE=/usr/include/asterisk
ASTERISK_MODULES=/usr/lib/asterisk/modules

# Source files
SOURCES=chan_moq.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=chan_moq.so

# Build rules
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -I$(ASTERISK_INCLUDE) -c $< -o $@

install: $(TARGET)
	install -m 755 $(TARGET) $(ASTERISK_MODULES)/
	@echo "Module installed. Run 'asterisk -rx \"module load chan_moq.so\"' to load"

uninstall:
	rm -f $(ASTERISK_MODULES)/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)

reload: all
	asterisk -rx "module unload chan_moq.so"
	$(MAKE) install
	asterisk -rx "module load chan_moq.so"

.PHONY: all install uninstall clean reload
