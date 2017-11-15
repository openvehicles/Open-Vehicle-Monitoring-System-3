# wolfssh
# Copyright (C) 2014-2017 wolfSSL Inc.
# All right reserved.

AC_INIT([wolfssh], [1.1.0], [http://wolfssl.com], [wolfssh])
AC_PREREQ([2.63])
AC_CONFIG_AUX_DIR([build-aux])

: ${CFLAGS=""}

AC_CANONICAL_HOST
AC_CANONICAL_BUILD

AM_INIT_AUTOMAKE([1.11 -Wall -Werror -Wno-portability foreign tar-ustar subdir-objects no-define color-tests])

AC_ARG_PROGRAM
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([src/config.h])

WOLFSSH_LIBRARY_VERSION=3:0:2
#                       | | |
#                +------+ | +---+
#                |        |     |
#               current:revision:age
#                |        |     |
#                |        |     +- increment if interfaces have been added
#                |        |        set to zero if interfaces have been removed
#                |        |        or changed
#                |        +- increment if source code has changed
#                |           set to zero if current is incremented
#                +- increment if interfaces have been added, removed or changed
AC_SUBST([WOLFSSH_LIBRARY_VERSION])

LT_PREREQ([2.2])
LT_INIT([disable-static], [win32-dll])
LT_LANG([C])

gl_VISIBILITY
AS_IF([test -n "$CFLAG_VISIBILITY"],
      [AM_CPPFLAGS="$AM_CPPFLAGS $CFLAG_VISIBILITY"
       CPPFLAGS="$CPPFLAGS $CFLAG_VISIBILITY"])

# silent
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CC_C_O
AC_PROG_INSTALL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_SIZEOF(long long, 8)
AC_CHECK_SIZEOF(long, 4)

# Check headers/libs
AC_CHECK_FUNCS([gethostbyname])
AC_CHECK_FUNCS([getaddrinfo])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([inet_ntoa])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([socket])
AC_CHECK_LIB(network,socket)

# Requirements
TAO_REQUIRE_LIBWOLFSSL
#REQUIRE_CYASSL([scep])
#REQUIRE_WOLFCRYPT([aes rsa dh])

# since we have autoconf available, we can use cyassl options header
AM_CPPFLAGS="$AM_CPPFLAGS -DHAVE_CYASSL_OPTIONS"

# DEBUG
DEBUG_CFLAGS="-g -O0"
DEBUG_CPPFLAGS="-DDEBUG -DDEBUG_WOLFSSH"

AX_DEBUG
AS_IF([test "x$ax_enable_debug" = "xyes"],
      [AM_CFLAGS="$DEBUG_CFLAGS $AM_CFLAGS"
       AM_CPPFLAGS="$DEBUG_CPPFLAGS $AM_CPPFLAGS"],
      [AM_CFLAGS="$AM_CFLAGS -O2"
       AM_CPPFLAGS="-DNDEBUG $AM_CFLAGS"])

AX_PTHREAD([AM_CFLAGS="$AM_CFLAGS $PTHREAD_CFLAGS"])


# Inline Build
AC_ARG_ENABLE([inline],
    [AS_HELP_STRING([--enable-inline],[Enable inline functions (default: enabled)])],
    [ENABLED_INLINE=$enableval],[ENABLED_INLINE=yes])

if test "$ENABLED_INLINE" = "no"
then
    AM_CFLAGS="$AM_CFLAGS -DNO_INLINE"
fi

AM_CONDITIONAL([BUILD_INLINE], [test "x$ENABLED_INLINE" = "xyes"])

# Key Generation
AC_ARG_ENABLE([keygen],
    [AS_HELP_STRING([--enable-keygen],[Enable key generation (default: disabled)])],
    [ENABLED_KEYGEN=$enableval],[ENABLED_KEYGEN=no])

AS_IF([test "x$ENABLED_KEYGEN" = "xyes"],
      [AM_CFLAGS="$AM_CFLAGS -DWOLFSSH_KEYGEN"])

AM_CONDITIONAL([BUILD_KEYGEN], [test "x$ENABLED_KEYGEN" = "xyes"])


# Checks for typedefs, structures, and compiler characteristics.
if test "$ac_cv_sizeof_long" = "8"; then
   AM_CPPFLAGS="$AM_CPPFLAGS -DSIZEOF_LONG=8"
else
    if test "$ac_cv_sizeof_long_long" = "8"; then
        AM_CPPFLAGS="$AM_CPPFLAGS -DSIZEOF_LONG_LONG=8"
    fi
fi

# HARDEN FLAGS
AX_HARDEN_CC_COMPILER_FLAGS

CREATE_HEX_VERSION
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])

# FINAL
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([wolfssh/version.h])

AX_CREATE_GENERIC_CONFIG
AX_AM_JOBSERVER([yes])

AC_OUTPUT

# force make clean
echo "---"
echo "Running make clean..."
make clean >/dev/null 2>&1
echo

# output config summary
echo "---"
echo "Configuration summary for $PACKAGE_NAME version $VERSION"
echo ""
echo "   * Installation prefix:       $prefix"
echo "   * System type:               $host_vendor-$host_os"
echo "   * Host CPU:                  $host_cpu"
echo "   * C Compiler:                $CC"
echo "   * C Flags:                   $CFLAGS"
echo "   * CPP Flags:                 $CPPFLAGS"
echo "   * Linker Flags:              $LDFLAGS"
echo "   * LIB Flags:                 $LIB"
echo
echo "   Features "
echo "   * keygen:                    $ENABLED_KEYGEN"

