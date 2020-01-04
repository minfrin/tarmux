# tarmux
A tool to multiplex and demultiplex streams from pipes to tar and back again.

Tarmux attempts to solve the problem of handling streamed data where there
is insufficient temporary space on disk to hold the data, as is forced by
tar.

In the simplest invocation, tarmux takes a stream from stdin, and creates
a tar file consisting of sequential numbered file fragments one after the
other. This can be reversed by the tardemux tool, which will untar the
fragments and reassemble the stream to stdout.

The tardemux tool only reads a single tar stream before exiting. This allows
multiple streams to be concatenated in the same stream, and then split out
by each invocation of tardemux.

Note: When compressing a tarmux stream, compress each individual component
of the stream separately and tarmux the result. If you do the inverse and
compress the tarmux the decompression will be greedy and force tarmux to
be greedy too. This is not a bug.

# example

If three separate sets of data are concatenated together as follows:

```
Little-Net:trunk minfrin$ cat ./test-source.sh 
#!/bin/bash

echo "one" | tarmux
echo "two" | tarmux
echo "three" | tarmux
```

The data can be split apart again back into the original three separate
streams as follows:

```
Little-Net:trunk minfrin$ cat ./test-sink.sh 
#!/bin/bash

tardemux
tardemux
tardemux
```

This results in the following output:

```
Little-Net:trunk minfrin$ ./test-source.sh | ./test-sink.sh 
one
two
three
```

Each invocation of tardemux can be piped to a separate pipeline for further
processing.

Optional files and/or pipes can be specified on the tarmux and tardemux
command lines to multiplex multiple streams into the same stream concurrently.

# downloads

mod_ical is available as RPMs through [COPR] as follows:

```
dnf copr enable minfrin/tarmux
```

# about tarmux

Tarmux is written for speed in portable C, and is tested on MacOSX, Redhat
and Ubuntu. Tar mux depends on libarchive http://www.libarchive.org/.
Packaging is available for RPM and Debian/Ubuntu systems.

# license

Tarmux is written by Graham Leggett, and is licensed under the Apache
Licence, Version 2.

  [COPR]: <https://copr.fedorainfracloud.org/coprs/minfrin/tarmux/>
