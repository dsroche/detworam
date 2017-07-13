CPPFLAGS = -std=c++11 -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -fmax-errors=1
CPPFLAGS += -ggdb -Og

EXPROGS = ramdisk

$(EXPROGS): %: %.cpp nbdserv.h
	g++ $(CPPFLAGS) $< -o $@
