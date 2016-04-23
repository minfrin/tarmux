#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(tarmux, 0.0.1, minfrin@sharp.fm)
AM_INIT_AUTOMAKE([dist-bzip2])
AC_CONFIG_FILES([Makefile tarmux.spec])
AC_CONFIG_SRCDIR([tarmux.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime])

# Checks for header files
AC_CHECK_HEADERS([archive_write_set_format_raw])

PKG_CHECK_MODULES(libarchive, libarchive >= 3.1)

CFLAGS="$CFLAGS $libarchive_CFLAGS"
CPPFLAGS="$CPPFLAGS $libarchive_CPPFLAGS"
LDFLAGS="$LDFLAGS $libarchive_LIBS"

AC_OUTPUT

