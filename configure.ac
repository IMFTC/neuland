AC_PREREQ([2.69])
AC_INIT([Neuland],
        [0.0.1],
        [github.com],
        [neuland],
        [github.com])
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_INIT_AUTOMAKE([-Wall foreign])
AC_PROG_CC
AM_PROG_CC_C_O

IT_PROG_INTLTOOL

GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"],
                   [GETTEXT package name])
AM_GLIB_GNU_GETTEXT

PKG_CHECK_MODULES(NEULAND,
        gtk+-3.0 >= 3.13
        glib-2.0 >= 2.40
        libtoxcore);

AC_CHECK_HEADERS([malloc.h stdlib.h string.h])

AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

AC_CONFIG_FILES([Makefile
                src/Makefile
                po/Makefile.in])

AC_OUTPUT
