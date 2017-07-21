CXX ?= g++
CPPFLAGS ?= -Wall -Wextra -Wno-unused-parameter
CXXFLAGS ?= -std=c++11 -pedantic
# fast options
CPPFLAGS += -march=native -Ofast
# debug options
# CPPFLAGS += -ggdb -Og -DNBDCPP_DEBUG -Werror -fmax-errors=1

EXPROGS = ramdisk loopback

all: $(EXPROGS)

$(EXPROGS): %: %.cpp nbdserv.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $< $(LOADLIBES) $(LDLIBS) -o $@

clean:
	rm -f $(EXPROGS)
