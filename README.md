# Deterministic Stash-Free Write-Only ORAM

This is the implementation of a new write-only ORAM scheme
that can be used to protect disk access patterns from an
attacker that can learn the entire history of *write* operations
to the underlying physical media.

Specifically, we have a C++ library that supports an
*encrypted virtual block device* based on
[nbd][], [BUSE][], and [mbedTLS][].

[nbd]: https://nbd.sourceforge.io/
[BUSE]: https://github.com/acozzette/BUSE
[mbedTLS]: https://tls.mbed.org/

The code and this repository are anonymized because there is
a research article associated with this implementation that is
currently under anonymous peer review. The authors are happy to
collaborate with you (and share their paper) in the meantime; you
can contact them by sending mail to <detworam@gmail.com>.

# Requirements

To build and use this package, you will need:

+   Network Block Device driver support in your version of \*nix
+   A C++14 compatible compiler
+   Root access in order to mount the device

Compatible versions/portions of [BUSE][] and [mbedTLS][]
are included as subdirectories in this repository, so you do not
need to download and install them separately.

In addition to the above, if you want to reproduce our benchmarks
you will also need:

+   Support for the [btrfs][] filesystem
+   The [bonnie++][] benchmarking utility
+   The [fio][] benchmarking utility

[btrfs]: https://btrfs.wiki.kernel.org/index.php/Main_Page
[bonnie++]: http://www.coker.com.au/bonnie++/
[fio]: https://github.com/axboe/fio

On a Debian-based system (including Ubuntu) you can install all of these
requirements by running the command as root:

    apt install nbd-client build-essential btrfs-tools bonnie++ fio

# Usage

To compile and run a large number of check programs on every part of the
`woram` C++ library, run

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

The `BUSE` folder contains a modified version of [BUSE][], which is
licensed under GPL version 2. See `BUSE/LICENSE` for details.

The `mbedtls` folder contains a subset of [mbedTLS][], which is licensed
under the Apache 2.0 license; see `mbedtls/LICENSE` for details.
