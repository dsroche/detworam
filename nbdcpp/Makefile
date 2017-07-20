CXX ?= g++
CPPFLAGS ?= -Wall -Wextra -Werror -Wno-unused-parameter -fmax-errors=1
CXXFLAGS ?= -std=c++11 -pedantic
CPPFLAGS += -ggdb -Og

EXPROGS = ramdisk loopback

all: $(EXPROGS)

$(EXPROGS): %: %.cpp nbdserv.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $< $(LOADLIBES) $(LDLIBS) -o $@

clean:
	rm -f $(EXPROGS)
