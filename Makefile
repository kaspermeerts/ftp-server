CC = gcc
DEFINES = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
WARNINGS = -Wextra -Wall -Wwrite-strings -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wstrict-aliasing -pedantic
CFLAGS = $(WARNING) $(DEFINES) -std=c99 -march=native -pipe -ggdb 
PROGNAME = ftpd
OBJECTS = daemon.o server.o util.o command.o config.o main.o child.o log.o state.o throttle.o vfs.o ls.o stream.o signals.o reply.o core.o auth.o
INCFLAGS =
LDFLAGS = -lcrypt

all: $(PROGNAME) ctags

$(PROGNAME): $(OBJECTS)
	@echo "	LD $(PROGNAME) "
	@$(CC) -o $(PROGNAME) $(OBJECTS) $(LDFLAGS)


.SUFFIXES:	.c .o 

.c.o :
	@echo "	CC $@"
	@$(CC) -o $@ -c $(CFLAGS) $< $(INCFLAGS)

clean:
	rm -f *.o $(PROGNAME)

ctags:
	@echo "	CTAGS"
	@ctags -R .

.PHONY: all clean ctags
