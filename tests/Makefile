CXXFLAGS += -std=c++14 -pedantic
CXXFLAGS += -I.. -I../mbedtls/include
CXXFLAGS += -Wall -Wextra -Werror
CXXFLAGS += -ggdb -Og
CXXFLAGS += -DWORAM_DEBUG
LDFLAGS += -L../mbedtls
LDLIBS += -lcr
DEPFLAGS = -MMD -MP -MQ $@

# detect compiler
ISCLANG = $(shell $(CXX) --version | grep -q 'clang' && echo yes || echo no)
ISGXX = $(shell $(CXX) --version | grep -q '\<g++' && echo yes || echo no)
ifeq "$(ISCLANG)" "yes"
CXXFLAGS += -ferror-limit=1
else
ifeq "$(ISGXX)" "yes"
CXXFLAGS += -fmax-errors=1
else
$(warning "WARNING: compiler not recognized as clang or g++")
endif
endif

TESTS = $(patsubst test_%.cpp,%,$(wildcard test_*.cpp))
CHECKS = $(TESTS:%=check_%)
VALGS = $(TESTS:%=valg_%)
DATAF = tenmeg.dat

UTILS =
UTIL_OBJS = $(UTILS:=.o)

all: $(TESTS:%=test_%) .gitignore
check: $(CHECKS)
	@echo "All $(words $^) checks passed."

tenmeg.dat:
	dd if=/dev/zero of=$@ bs=1024 count=10000
test_test test_filemem: | $(DATAF)

%: %.cpp $(UTIL_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPFLAGS) $< $(UTIL_OBJS) -o $@ $(LDLIBS)

$(UTIL_OBJS): %.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $<

$(CHECKS): check_%: test_%
	@echo -n "Running $<... "
	@./$<

$(VALGS): valg_%: test_%
	@echo -n "Running $<... "
	@valgrind --leak-check=full ./$<

.gitignore: Makefile
	echo $(TESTS:%=test_%) $(DATAF) | tr ' ' '\n' | sort > $@

clean:
	rm -f $(TESTS:%=test_%.o) $(TESTS:%=test_%) $(TESTS:%=test_%.d) $(DATAF)

.PHONY: all clean $(CHECKS) $(VALGS)

-include $(wildcard *.d)

# for debugging the makefile
# source: http://www.drdobbs.com/tools/debugging-makefiles/
print-%:
	@$(error $* is $($*) ($(value $*)) (from $(origin $*)))
