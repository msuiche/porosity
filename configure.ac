AC_INIT([porosity],[1.0.0])
AC_PROG_CXX
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])
AC_CONFIG_FILES([Makefile porosity/porosity/Makefile])
AM_INIT_AUTOMAKE([foreign])
LT_INIT()

AC_SUBST([AM_CXXFLAGS], [-std=c++1y])

PKG_PROG_PKG_CONFIG
if test -z "$PKG_CONFIG"; then
   AC_MSG_ERROR([pkg-config not found. pkg-config is required to check for some dependencies.])
fi

AC_OUTPUT
