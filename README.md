# Deterministic Stash-Free Write-Only ORAM

This is the implementation of a new write-only ORAM scheme
that can be used to protect disk access patterns from an
attacker that can learn the entire history of *write* operations
to the underlying physical media.

Specifically, we have a C++ library that supports an
*encrypted virtual block device* using
[nbdcpp][] and [mbedTLS][].

[nbd]: https://nbd.sourceforge.io/
[nbdcpp]: https://github.com/dsroche/nbdcpp
[mbedTLS]: https://tls.mbed.org/

This implementation is an academic project based at the U.S. Naval
Academy based on research into efficient techniques to preserve privacy
in cloud computing applications. You can read our paper for a detailed
description and analysis:
<https://arxiv.org/abs/1706.03827>

This project was supported by the U.S. National Science Foundation under
[award \#1618269](https://www.nsf.gov/awardsearch/showAward?AWD_ID=1618269).

# Requirements

To build and use this package, you will need:

+   Network Block Device driver support in your version of \*nix
+   The `nbd-client` program available in most package managers,
    part of the [nbd project][nbd]
+   A C++14 compatible compiler
+   Root access in order to mount the device

Compatible versions/portions of [nbdcpp][] and [mbedTLS][]
are included as submodules of this repository. If you clone using the
`--recursive` option, you will download this repository as well as the
subdirectories. If not, just run `git submodule init` and then `git
submodule update` to download them after cloning the `detworam`
repository itself.

In addition to the above, if you want to reproduce our benchmarks
you will also need:

+   Support for the [btrfs][] filesystem
+   The [bonnie++][] benchmarking utility
+   The [fio][] benchmarking utility

[btrfs]: https://btrfs.wiki.kernel.org/index.php/Main_Page
[bonnie++]: http://www.coker.com.au/bonnie++/
[fio]: https://github.com/axboe/fio

On a Debian-based system (including Ubuntu) you can install all of these
requirements by running the following command as root:

    apt install nbd-client build-essential btrfs-tools bonnie++ fio

# Usage

To compile and run a large number of check programs on every part of the
`detworam` C++ library, run

    make check

# License

We hope that you might find something here useful, and that you will be
able to use it. We have attempted to give the broadest possible latitude
in that regard. There are absolutely no restrictions on the use of this
software except where required, and we only ask (but do not require!)
that you acknowledge the contribution and let us know if you use this
code in something.

Except where otherwise specified, all files in this repository are
released into the public domain according to the
[Unlicense](http://unlicense.org/); see `LICENSE` for details.

The `mbedtls` submodule contains a subset of [mbedTLS][], which is licensed
under the Apache 2.0 license; see `mbedtls/LICENSE` for details.
