#
#   Empire - A multi-player, client/server Internet based war game.
#   Copyright (C) 1986-2015, Dave Pare, Jeff Bailey, Thomas Ruschak,
#                 Ken Stevens, Steve McClure, Markus Armbruster
#
#   Empire is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#   ---
#
#   See files README, COPYING and CREDITS in the root of the source
#   tree for related information and legal notices.  It is expected
#   that future projects/authors will amend these files as needed.
#
#   ---
#
#   configure.ac: Autoconf input file
#
#   Known contributors to this file:
#      Markus Armbruster, 2005-2015
#
# Process this file with autoconf to produce a configure script.

# Autoconf makes checking for and programming around assorted ancient
# crap relatively painless.  But why bother?  Just rely on C89 and
# POSIX, and when something breaks on some oddball machine, see
# whether it's worth fixing.

AC_PREREQ(2.64)
AC_INIT([Empire Client], [4.3.34], [wolfpack@wolfpackempire.com],,
	[http://www.wolfpackempire.com/])
AC_CONFIG_SRCDIR([empire.6])
AC_CONFIG_HEADERS([config.h])


### Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Not a program, but need to check this early
MY_WINDOWS_API


### Checks for libraries.
LIBS="$LIBS_SOCKETS $LIBS"
AX_LIB_SOCKET_NSL
if test "$Windows_API" = yes; then
	AC_LIBOBJ([w32/w32io])
	AC_LIBOBJ([w32/w32sockets])
fi


### Checks for header files.


### Checks for typedefs, structures, and compiler characteristics.

if test "$Windows_API" = yes; then
	CPPFLAGS="$CPPFLAGS -Iw32"
	CFLAGS="$CFLAGS -mthreads"
fi


### Checks for library functions.

AC_CHECK_FUNCS(getaddrinfo getpass)


### Site configuration

AC_ARG_VAR([EMPIREHOST], [Default host [blitz.wolfpackempire.com]])
test "$EMPIREHOST" || EMPIREHOST=blitz.wolfpackempire.com

AC_ARG_VAR([EMPIREPORT], [Default port [6789]])
test "$EMPIREPORT" || EMPIREPORT=6789

MY_WITH_TERMINFO

### Output

AC_CONFIG_FILES([Makefile ipglob.c])
AC_OUTPUT

AC_MSG_NOTICE([])
AC_MSG_NOTICE([-= Configuration summary =-])
AC_MSG_NOTICE([      terminfo: $with_terminfo])
AC_MSG_NOTICE([    EMPIREHOST: $EMPIREHOST])
AC_MSG_NOTICE([    EMPIREPORT: $EMPIREPORT])
