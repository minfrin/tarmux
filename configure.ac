#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([tarmux],[1.0.5],[minfrin@sharp.fm])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([dist-bzip2])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([tarmux.c])
AC_CONFIG_MACRO_DIR([m4])
LT_INIT

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

AC_CONFIG_FILES([Makefile tarmux.spec])
AC_SUBST([LIBTOOL_DEPS])
AC_OUTPUT

