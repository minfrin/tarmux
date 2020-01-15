
AM_CFLAGS = ${libarchive_CFLAGS}
ACLOCAL_AMFLAGS = "-Im4"
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

bin_PROGRAMS = tarmux tardemux
tarmux_SOURCES = tarmux.c
tarmux_LDADD = ${libarchive_LIBS}
tardemux_SOURCES = tardemux.c
tardemux_LDADD = ${libarchive_LIBS}

EXTRA_DIST = tarmux.spec debian/changelog debian/compat debian/control debian/copyright debian/docs debian/tarmux.dirs debian/rules debian/source/format
dist_man_MANS = tarmux.1 tardemux.1

tarmux.1: tarmux.c $(top_srcdir)/configure.ac
	which help2man && help2man -n "Multiplex streams using tar file fragments." ./tarmux > tarmux.1 || true

tardemux.1: tardemux.c $(top_srcdir)/configure.ac
	which help2man && help2man -n "Demultiplex streams using tar file fragments." ./tardemux > tardemux.1 || true

