
#--------------------------------------------------------------------
# Check for libwolfssl
#--------------------------------------------------------------------


AC_DEFUN([_TAO_SEARCH_LIBWOLFSSL],[
  AC_REQUIRE([AC_LIB_PREFIX])

  LDFLAGS="$LDFLAGS -L/usr/local/lib"
  LIBS="$LIBS -lwolfssl"

  AC_LIB_HAVE_LINKFLAGS(wolfssl,,
  [
    #include <wolfssl/wolfcrypt/wc_port.h>
  ],[
    wolfCrypt_Init();
  ])

  AM_CONDITIONAL(HAVE_LIBWOLFSSL, [test "x${ac_cv_libwolfssl}" = "xyes"])

  AS_IF([test "x${ac_cv_libwolfssl}" = "xyes"],[
    save_LIBS="${LIBS}"
    LIBS="${LIBS} ${LTLIBWOLFSSL}"
    AC_CHECK_FUNCS(wolfSSL_Cleanup)
    LIBS="$save_LIBS"
  ])
])

AC_DEFUN([_TAO_HAVE_LIBWOLFSSL],[

  AC_ARG_ENABLE([libwolfssl],
    [AS_HELP_STRING([--disable-libwolfssl],
      [Build with libwolfssl support @<:@default=on@:>@])],
    [ac_enable_libwolfssl="$enableval"],
    [ac_enable_libwolfssl="yes"])

  _TAO_SEARCH_LIBWOLFSSL
])


AC_DEFUN([TAO_HAVE_LIBWOLFSSL],[
  AC_REQUIRE([_TAO_HAVE_LIBWOLFSSL])
])

AC_DEFUN([_TAO_REQUIRE_LIBWOLFSSL],[
  ac_enable_libwolfssl="yes"
  _TAO_SEARCH_LIBWOLFSSL

  AS_IF([test x$ac_cv_libwolfssl = xno],[
    AC_MSG_ERROR([libwolfssl is required for ${PACKAGE}, It can be obtained from http://www.wolfssl.com/download.html/])
  ])
])

AC_DEFUN([TAO_REQUIRE_LIBWOLFSSL],[
  AC_REQUIRE([_TAO_REQUIRE_LIBWOLFSSL])
])
