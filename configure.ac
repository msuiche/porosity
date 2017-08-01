AC_INIT([porosity],[1.0.0])
AC_PROG_CXX
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])
AC_CONFIG_FILES([Makefile porosity/porosity/Makefile])
AM_INIT_AUTOMAKE([foreign])
LT_INIT()

AC_SUBST([AM_CXXFLAGS], [-std=c++1y])

AC_CHECK_HEADERS([boost/version.hpp], [boost_found_headers=yes; break;])
AS_IF([test "x$boost_found_headers" != "xyes"],
[AC_MSG_ERROR([Unable to find boost headers.])])

define(MINIMUM_REQUIRED_BOOST, 1.62.0)
AX_BOOST_BASE([MINIMUM_REQUIRED_BOOST])

AC_MSG_CHECKING([boost version])
TEMP_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$BOOST_CPPFLAGS $CPPFLAGS"
AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[
    @%:@include <boost/version.hpp>
  ]], [[
    #if BOOST_VERSION > 105800
    // Everything is okay
    #else
    #  error Boost version is too old
    #endif
  ]])],[
    AC_MSG_RESULT(yes)
  ],[
    AC_MSG_ERROR([boost versions <= 1.58 are known to be broken for multiprecision number comparison.])
])
CPPFLAGS="$TEMP_CPPFLAGS"

AC_OUTPUT
