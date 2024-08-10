TARGET = yeahconsole
CC = gcc
#CC = cc
INSTALL = install

PREFIX = /usr/local

LIBS = -lX11 -lXrandr
INCLUDES = -I/usr/X11R6/include
LIB_DIRS = -L/usr/X11R6/lib
FLAGS = -Os -Wall -g

OBJECTS := yeahconsole.o
SOURCES := yeahconsole.c

$(TARGET): $(OBJECTS)
	$(CC) $(DEFINES) $(INCLUDES) $(LIB_DIRS) -o $@ $< $(LIBS)
	# strip $@

$(OBJECTS): $(SOURCES)
	$(CC) $(FLAGS)  $(DEFINES) $(INCLUDES) $(LIB_DIRS) -c -o $@ $<

clean:
	rm -rf $(TARGET) $(OBJECTS)

install: $(TARGET) $(MAN)
	$(INSTALL) -d $(PREFIX)/bin
	$(INSTALL) -m 0755 $(TARGET) $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
