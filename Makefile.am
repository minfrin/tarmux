
AM_CFLAGS = ${libarchive_CFLAGS}

bin_PROGRAMS = tarmux tardemux
tarmux_SOURCES = tarmux.c
tarmux_LDADD = ${libarchive_LIBS}
tardemux_SOURCES = tardemux.c
tardemux_LDADD = ${libarchive_LIBS}

EXTRA_DIST = tarmux.spec
dist_man_MANS = tarmux.1 tardemux.1

tarmux.1: tarmux
	which help2man && help2man ./tarmux > tarmux.1 || true

tardemux.1: tardemux
	which help2man && help2man ./tardemux > tardemux.1 || true

