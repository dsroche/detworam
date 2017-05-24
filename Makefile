all: buse crypto tests

buse:
	make -C BUSE

crypto:
	make -C mbedtls

check: tests
	make -C tests check

tests: crypto
	make -C tests

clean:
	make -C BUSE clean
	make -C mbedtls clean
	make -C tests clean

.PHONY: all clean buse crypto check tests
