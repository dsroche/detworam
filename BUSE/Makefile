CPPFLAGS += -Wall -Wextra -Werror
CFLAGS += -std=c99 -pedantic
LDFLAGS += -L.
LDLIBS += -lbuse
DEPFLAGS = -MMD -MP -MQ $@
CC ?= gcc

# debugging options
#CPPFLAGS += -ggdb -DWORAM_DEBUG
# high-speed options
CPPFLAGS += -Ofast -march=native

LIBRARY = libbuse.a
PROGS = busexmp loopback

all: $(LIBRARY) $(PROGS)

$(PROGS:=.o): buse.h

$(LIBRARY): buse.o buse.h
	ar rcs $@ $^

clean:
	rm -f $(LIBRARY) $(PROGS) *.o

.PHONY: all clean
