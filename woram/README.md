Deterministic Write-Only ORAM C++ Library
=========================================

The files in this folder constitute a header-only C++14
library for the `detworam` package.

There is nothing to build here; you just `#include` the
relevant header file in your program.

The cryptographic functionality in `crypto.h` requires the
mbed TLS library, which should be included in the subfolder
`mbedtls` in this same repository.

Usage
-----

The two main types of classes in this library are *memories* and
*worams*.

Any class satisfying the `is_memory` trait can be used as
back-end storage: the two most important ones are `FileMem`, which uses
any given filename, and `LocalMem`, which uses allocated memory.

Any class satisfying the `is_woram` trait is a write-only ORAM. While
their specific constructions vary, all should have a *factory* helper
struct with a `create` method that can be used to instantiate an
instance of that class.

For example, to create a deterministic write-only ORAM, using AES
encryption and a Trie position map, backed by a 10MB file called `data.dat`,
using block size 4KB and logical size 5MB, you could use the following
program:

    #include <woram/filemem.h>
    #include <woram/triepm.h>

    int main() {
      // typedef for 10MB file with 4KB blocks
      using Mem = woram::FileMem<4 * (1ul << 10), 10 * (1ul << 20)>;
      // factory to create 5MB write-only ORAM backed by Mem
      using Factory = woram::DetWoCryptTrie<>::Factory<Mem, 5 * (1ul << 20)>;

      // instantiate the ORAM
      auto m = Factory::create("data.dat");

      // now you can call m->load(...) and m->store(...) to access contents

      return 0;
    }

License
-------

The header files here are released into the public domain (where
possible) under the terms of the [Unlicense](http://unlicense.org/).
See the `LICENSE` file for details.
