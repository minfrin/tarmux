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

# about tarmux

Tarmux is written for speed in portable C, and is tested on MacOSX, Redhat
and Ubuntu. Tar mux depends on libarchive http://www.libarchive.org/.
Packaging is available for RPM and Debian/Ubuntu systems.
