Deterministic Write-Only ORAM library test programs
===================================================

The programs here can be used to check the correctness
of the `detworam` C++14 header library.

Some tests depend on the mbed TLS library, which should
be included in the subfolder `mbedtls` in this same repository.

Usage
-----

First, be sure to run `make` in the `mbedtls` folder so that
the static library `mbedtls/libcr.a` is created.

Then run `make check` to compile and run all of the test
programs.

Because many different instantiations of the relevant template classes
are made in each test program, some of them take a few minutes to
compile.

License
-------

The source code here is released into the public domain (where
possible) under the terms of the [Unlicense](http://unlicense.org/).
See the `LICENSE` file for details.
