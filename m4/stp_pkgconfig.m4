dnl @synopsis STP_CONFIG_PKGCONFIG_IN [(LIBRARY, SHORT_NAME [, DESCRIPTION [, DESTINATION]])]
dnl
dnl Creates a custom pkg-config script.  The script supports
dnl --cflags, --libs and --version options.
dnl
dnl This macro saves you all the typing for a pkg-config.in script;
dnl you don't even need to distribute one along. Place this macro
dnl in your configure.ac, et voila, you got one that you want to install.
dnl
dnl The options:
dnl $1 = LIBRARY	e.g. gtk, ncurses, gimpprint
dnl $2 = SHORT_NAME     e.g. GTK+, Gimp-Print
dnl $3 = DESCRIPTION    one line description of library
dnl $4 = DESTINATION	directory path, e.g. src/scripts
dnl
dnl It is suggested that the following CFLAGS and LIBS variables are
dnl used in your configure.ac.  library_libs is *essential*.
dnl library_cflags is important, but not always needed.  If they do not
dnl exist, defaults will be taken from LIBRARY_CFLAGS, LIBRARY_LIBS
dnl (should be -llibrary *only*) and LIBRARY_LIBDEPS (-l options for
dnl libraries your library depends upon.
dnl LIBLIBRARY_LIBS is simply $LIBRARY_LIBS $LIBRARY_LIBDEPS.
dnl NB. LIBRARY and library are the name of your library, in upper and
dnl lower case repectively e.g. GTK, gtk.
dnl
dnl Uppercase names are for your own use during the package build while
dnl the lowercase names are to use the the generated pkg-config file.
dnl Some of the uppercase names could be used to guess the lowercase
dnl ones, but this is not recommended -- create them yourself.
dnl
dnl LIBRARY_CFLAGS:    cflags for compiling libraries and example progs
dnl LIBRARY_LIBS:      libraries for linking programs
dnl LIBRARY_LIBDEPS*:  libraries for linking libraries against (needed
dnl                    to link -static
dnl library_requires:  packages required by your library
dnl library_conflicts: packages to conflict with your library
dnl library_cflags*:   cflags to store in library-config
dnl library_libs*:     libs to store in library-config
dnl LIBLIBRARY_LIBS:   libs to link programs IN THIS PACKAGE ONLY against
dnl library_version*:  the version of your library (x.y.z recommended)
dnl   *=required if you want sensible output, otherwise they will be
dnl     *guessed* (DWIM, but usually correct)
dnl
dnl There is also an AC_SUBST(LIBRARY_PKGCONFIG) that will be set to
dnl the name of the file that we output in this macro. Use as:
dnl
dnl  install-data-local: install-pkgconfig
dnl  install-pkgconfig:
dnl     $(mkinstalldirs) $(DESTDIR)$(bindir)
dnl     $(INSTALL_DATA) @LIBRARY_PKGCONFIG@ $(DESTDIR)$(prefix)/lib/pkgconfig
dnl
dnl Or, if using automake:
dnl
dnl  pkgconfigdatadir = $(prefix)/lib/pkgconfig
dnl  pkgconfigdata_DATA = gimpprint.pc = @LIBRARY_PKGCONFIG@
dnl
dnl Example usage:
dnl
dnl GIMPPPRINT_LIBS="-lgimpprint"
dnl AC_CHECK_LIB(m,pow,
dnl              GIMPPRINT_LIBDEPS="${GIMPPRINT_LIBDEPS} -lm")
dnl STP_CONFIG_PKGCONFIG_IN([gimpprint], [Gimp-Print], [GIMP Print Top Quality Printer Drivers], [src/main])
dnl
dnl @version $Id: stp_pkgconfig.m4,v 1.3 2003/01/01 21:06:02 rleigh Exp $
dnl @author Roger Leigh <roger@whinlatter.uklinux.net>
dnl
## STP_CONFIG_PKGCONFIG_IN(LIBRARY, DESCRIPTION, DESTINATION)
## -----------------------
## Create a custom pkg-config script for LIBRARY.  Include a one-line
## DESCRIPTION.  The script will be created in a DESTINATION
## directory.
AC_DEFUN([STP_CONFIG_PKGCONFIG_IN],
[# create a custom pkg-config file ($1.pc.in)
m4_pushdef([PKGCONFIG_DIR], [m4_if([$4], , , [$4/])])
PKGCONFIG_FILE="PKGCONFIG_DIR[]$1.pc.in"
AC_SUBST(target)dnl
AC_SUBST(host)dnl
AC_SUBST(build)dnl
dnl create directory if it does not preexist
m4_if([$4], , , [AS_MKDIR_P([$4])])
AC_MSG_NOTICE([creating $PKGCONFIG_FILE])
dnl we're going to need uppercase, lowercase and user-friendly versions of the
dnl string `MODULE'
m4_pushdef([MODULE_UP], m4_translit([$1], [a-z-], [A-Z_]))dnl
m4_pushdef([MODULE_DOWN], m4_translit([$1], [A-Z-], [a-z_]))dnl
if test -z "$MODULE_DOWN[]_cflags" ; then
  if test -n "$MODULE_UP[]_CFLAGS" ; then
      MODULE_DOWN[]_cflags="$MODULE_UP[]_CFLAGS"
  else
dnl    AC_MSG_WARN([variable `MODULE_DOWN[]_cflags' undefined])
    MODULE_DOWN[]_cflags=''
  fi
fi
AC_SUBST(MODULE_DOWN[]_cflags)dnl
if test -z "$MODULE_DOWN[]_libs" ; then
  if test -n "$MODULE_UP[]_LIBS" ; then
    MODULE_DOWN[]_libs="$MODULE_UP[]_LIBS"
  else
    AC_MSG_WARN([variable `MODULE_DOWN[]_libs' and `MODULE_UP[]_LIBS' undefined])
    MODULE_DOWN[]_libs='-l$1'
  fi
  if test -n "$MODULE_UP[]_LIBDEPS" ; then
    MODULE_DOWN[]_libs="$MODULE_DOWN[]_libs $MODULE_UP[]_LIBDEPS"
  fi
fi			    
AC_SUBST(MODULE_DOWN[]_libs)dnl
AC_SUBST(MODULE_DOWN[]_requires)
AC_SUBST(MODULE_DOWN[]_conflicts)
if test -z "$MODULE_DOWN[]_version" ; then
  AC_MSG_WARN([variable `MODULE_DOWN[]_version' undefined])
  MODULE_DOWN[]_version="$VERSION"
fi
AC_SUBST(MODULE_DOWN[]_version)dnl
echo 'prefix=@prefix@' >$PKGCONFIG_FILE
echo 'exec_prefix=@exec_prefix@' >>$PKGCONFIG_FILE
echo 'libdir=@libdir@' >>$PKGCONFIG_FILE
echo 'includedir=@includedir@' >>$PKGCONFIG_FILE
echo '' >>$PKGCONFIG_FILE
echo 'Name: $2' >>$PKGCONFIG_FILE
echo 'Description: $3' >>$PKGCONFIG_FILE
if test -n "$MODULE_DOWN[]_requires" ; then
  echo 'Requires: @MODULE_DOWN[]_requires@' >>$PKGCONFIG_FILE
fi
if test -n "$MODULE_DOWN[]_conflicts" ; then
  echo 'Conflicts: @MODULE_DOWN[]_requires@' >>$PKGCONFIG_FILE
fi
echo 'Version: @MODULE_DOWN[]_version@' >>$PKGCONFIG_FILE
echo 'Libs: -L${libdir} @MODULE_DOWN[]_libs@' >>$PKGCONFIG_FILE
echo 'Cflags: -I${includedir} @MODULE_DOWN[]_cflags@' >>$PKGCONFIG_FILE
m4_pushdef([PKGCONFIG_UP], [m4_translit([$1], [a-z-], [A-Z_])])dnl
PKGCONFIG_UP[]_PKGCONFIG="PKGCONFIG_DIR[]$1-config"
AC_SUBST(PKGCONFIG_UP[]_PKGCONFIG)
dnl AC_CONFIG_FILES(PKGCONFIG_DIR[]$1[-config], [chmod +x ]PKGCONFIG_DIR[]$1[-config])
m4_popdef([PKGCONFIG_UP])
m4_popdef([MODULE_DOWN])dnl
m4_popdef([MODULE_UP])dnl
m4_popdef([PKGCONFIG_DIR])dnl
])
