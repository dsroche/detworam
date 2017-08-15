# nbdcpp: Network Block Device drivers in userspace C++

## Overview

This project lets you write your own block device drivers, through a simple
and efficient C++ interface, using only userspace tools. It relies on
the NBD (Network Block Device) system, which is part of the Linux
kernel. Some other projects with similar aims are [BUSE] and [nbdkit].

[BUSE]: https://github.com/acozzette/BUSE
[nbdkit]: https://github.com/libguestfs/nbdkit

This package provides a C++ template class that can be used to create
a very efficient userspace-level NBD (Network Block Device) server, as
well as a script to start up such a server and connect to it locally,
which allows you to create a new block device driver without writing any
kernel code.

The content of the library is really just two files:

+   `nbdserv.h`: A C++ header file. It contains template classes to turn
    a single class implementing simple read/write methods into an NBD
    server.

+   `makedev`: A bash script to launch an NBD server locally and connect
    to it using the `nbd-client` program.

`nbdserv.h` provides similar functionality to [nbdkit], in a
stripped-down header-only library. Using that header file and then
running the `makedev` script provides similar functionality to [BUSE] in
creating a custom block device running in userspace.

## Requirements

Because `nbdserv.h` is a header-only library, **there is no compilation
or library installation to use nbdcpp**. However, using this project
requires the following to be installed:

+   A recent Linux kernel which includes the [nbd] module.
+   A compiler with C++11 support.
+   The [nbd] userspace tools, notably `nbd-client`.

On a Debian or Ubuntu system with `g++` or `clang++` installed, the only
other package you probably need is `nbd-client`.

## Usage

To create a NBD server, all you need to do is create a C++ program with:

+   An `include` of `nbdserv.h`
+   A server class with your read/write methods and a few others, as
    specified in the `DevExample` class that can be found in the
    `nbdserv.h` file itself.
+   A main method that calls `nbdcpp_main` templatized on your class.

Compiling this program gives you an executable which, when run, launches
an NBD server listening on a specified unix socket or network port.

If you want to create your own block device on a single
machine, run the script `makedev` with your compiled NBD server
executable as an argument. This will launch your server and connect to
it using a unix socket, resulting in a device such as `/dev/nbd0` which
can then be used like any other block device.

The `makedev` script also creates a corresponding script with a name
like `stop-nbd0` which will, in the correct order, disconnect the nbd
device, stop the running server, and then delete the script itself.

## Examples

The repository includes two example nbdcpp servers, in the files
`ramdisk.cpp` and `loopback.cpp`.

To create a 8MB device in program memory (RAM), you would do:

    make ramdisk
    ./makedev ramdisk 8000

This will tell you which device name is being used --- most likely
`/dev/nbd0` --- and also create a script to destroy and disconnect the
device.

To create a loopback device providing direct access to an existing file
called `image.data`, you would do:

    make loopback
    ./makedev loopback image.data

After this, you can use your block device just like any hard disk
partition. The most likely thing to do on initial device creation would
be to format it according to your favorite fileystem and then mount it,
by running (as root):

    mkfs -t ext2 /dev/nbd0
    mount /dev/nbd0 your/mount/point

Then of course you would want to be sure to `umount` the device before
running the `stop-nbd0` command to disconnect from the local NBD server.

# Contributors and License

This package was initially written by
[Daniel S. Roche](http://www.usna.edu/cs/roche/). It was inspired by
the [BUSE] project by Adam Cozzette.

Anyone is welcome to contribute with a pull request or by forking. You
can also email to report any bugs, although this is an academic project
so no promises on regular maintenance.

All files in this repository are released into the public domain
according to the [Unlicense](http://unlicense.org/); see `LICENSE` for
details.
