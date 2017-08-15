SUBMODULES = mbedtls nbdcpp

all: $(SUBMODULES) tests

$(SUBMODULES:=/README.md):
	git submodule init
	git submodule update

$(SUBMODULES): %: %/README.md

mbedtls:
	make -C $@

check: tests
	make -C tests check

tests: mbedtls
	make -C tests

clean:
	make -C mbedtls clean
	make -C tests clean

.PHONY: all clean check tests $(SUBMODULES)
