CPPFLAGS += -I.
CPPFLAGS += -Wall -Wextra -Werror
LDFLAGS += -L.
LDLIBS += -lcr
DEPFLAGS = -MMD -MP -MQ $@
CC ?= gcc

# debugging options
#CPPFLAGS += -ggdb -DWORAM_DEBUG
# high-speed options
CPPFLAGS += -Ofast -march=native

LIBRARY=libcr.a

all: $(LIBRARY)

$(LIBRARY): aes.o aesni.o entropy.o entropy_poll.o ctr_drbg.o sha512.o timing.o
	ar rcs $@ $^

clean:
	rm -f $(LIBRARY) *.o

.PHONY: all clean
