#===============================================================================
#
# Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
#
# This program is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General
# Public License along with this program; if not, write to the Free
# Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# Correspondence concerning GBT software should be addressed as follows:
#   GBT Operations
#   National Radio Astronomy Observatory
#   P. O. Box 2
#   Green Bank, WV 24944-0002 USA
#
#===============================================================================
#===============================================================================

# Process this file with autoconf to produce a configure script.

# Prelude.
AC_PREREQ([2.59])

# required - args are: Project name, version, contacts
AC_INIT([Matrix], [0.1], [jbrandt AT nrao.edu, rcreager AT nrao.edu])


AC_CONFIG_MACRO_DIR([m4])

# unique source file --- primitive safety check
AC_CONFIG_SRCDIR([src/Architect.cc])

# place to put some extra build scripts installed
#AC_CONFIG_AUX_DIR([build-aux])

# fairly severe build strictness
# change foreign to gnu or gnits to comply with gnu standards
AM_INIT_AUTOMAKE([-Wall -Werror foreign 1.9.6])

# Checks for programs.
# Project uses plain C source
AC_PROG_CC
# Project has C++ source
AC_PROG_CXX
# Project build libraries
AC_PROG_LIBTOOL

# Checks for libraries.

# This macro is defined in check.m4 and tests if check.h and
# libcheck.a are installed in your system. It sets CHECK_CFLAGS and
# CHECK_LIBS accordingly.
#  AM_PATH_CHECK([MINIMUM-VERSION,
#                [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
AM_PATH_CHECK()

# C++ 11 support?
AX_CXX_COMPILE_STDCXX_11(noext,optional)
#AX_CXX_COMPILE_STDCXX_0X
AC_CHECK_DEFINE(-std=c++11, ,CXXFLAGS="$CXXFLAGS -std=c++0x")

############################## ZEROMQ ###################################
# Check for zeromq library
zeromq_prefix=detect
AC_ARG_WITH([zeromq],
    [AS_HELP_STRING([--with-zeromq=PREFIX],
        [build with ZeroMQ library installed in PREFIX [default=autodetect]])],
    [case "x$withval" in
        xno)
            AC_MSG_ERROR([matrix requires the ZeroMQ library])
            ;;
        xyes|x)
            ;;
        *)
            CPPFLAGS="$CPPFLAGS -I${withval}/include"
            LDFLAGS="$LDFLAGS -L${withval}/lib"
            zeromq_prefix=${withval}
            ;;
    esac ]
)
if test "x$zeromq_prefix" = "xdetect"; then
    PKG_CHECK_MODULES(
        [ZeroMQ], [libzmq], [zeromq_prefix=pkgconfig], [zeromq_prefix=])
        if test "x$zeromq_prefix" = "xpkgconfig"; then
            CPPFLAGS="$CPPFLAGS ${ZeroMQ_CFLAGS}"
            LDFLAGS="$LDFLAGS ${ZeroMQ_LIBS}"
        fi
fi

AC_ARG_ENABLE([version],
              [AS_HELP_STRING([--enable-version],
                              [builds with version information embedded [default=yes]])])
AM_CONDITIONAL(DISABLE_VERSION, test "x$enable_version" = "xno")

AC_CHECK_HEADER([zmq.h], [],
    [AC_MSG_ERROR([cannot find zmq.h])])

AC_ARG_ENABLE([self-contained],
              [AS_HELP_STRING([--enable-self-contained],
                              [generates a library that embeds all its external dependencies [default=no]])])

AC_ARG_ENABLE([self-contained-uuid],
              [AS_HELP_STRING([--enable-self-contained-uuid],
              [embeds the libuuid dependency in the shared object [default=no]])])

AC_CHECK_LIB([zmq], [zmq_init],
             [
             if test "x$enable_self_contained" = "xyes"; then
                    LDFLAGS="$LDFLAGS -Wl,-Bstatic -Wl,-lzmq -Wl,-Bdynamic"
             else
                     LIBS="-lzmq $LIBS"
             fi
             if test "x$enable_self_contained_uuid" = "xyes"; then
                     LDFLAGS="$LDFLAGS -Wl,-luuid"
             fi
             ],
    [AC_MSG_ERROR([cannot link with -lzmq])])

############################## YAML ###################################

# Check for yaml-cpp library
yamlcpp_prefix=detect
AC_ARG_WITH([yamlcpp],
    [AS_HELP_STRING([--with-yamlcpp=PREFIX],
        [build with yaml-cpp library installed in PREFIX [default=autodetect]])],
    [case "x$withval" in
        xno)
            AC_MSG_ERROR([matrix requires the yaml-cpp library])
            ;;
        xyes|x)
            ;;
        *)
            CPPFLAGS="$CPPFLAGS -I${withval}/include/yaml-cpp -I${withval}/include"
            LDFLAGS="$LDFLAGS -L${withval}/lib"
            yamlcpp_prefix=${withval}
            ;;
    esac ]
)
if test "x$yamlcpp_prefix" = "xdetect"; then
    PKG_CHECK_MODULES(
        [Yamlcpp], [libyamlcpp], [yamlcpp_prefix=pkgconfig], [yamlcpp_prefix=])
        if test "x$yamlcpp_prefix" = "xpkgconfig"; then
            CPPFLAGS="$CPPFLAGS ${Yamlcpp_CFLAGS}"
            LDFLAGS="$LDFLAGS ${Yamlcpp_LIBS}"
        fi
fi

AC_ARG_ENABLE([version],
              [AS_HELP_STRING([--enable-version],
                              [builds with version information embedded [default=yes]])])
AM_CONDITIONAL(DISABLE_VERSION, test "x$enable_version" = "xno")

AC_LANG_PUSH([C++])
AC_CHECK_HEADER([yaml-cpp/yaml.h], [],
    [AC_MSG_ERROR([cannot find yaml-cpp/yaml.h])])

AC_ARG_ENABLE([self-contained],
              [AS_HELP_STRING([--enable-self-contained],
                              [generates a library that embeds all its external dependencies [default=no]])])

AC_ARG_ENABLE([self-contained-uuid],
              [AS_HELP_STRING([--enable-self-contained-uuid],
              [embeds the libuuid dependency in the shared object [default=no]])])

AC_CHECK_LIB([yaml-cpp], [_init],
             [
             if test "x$enable_self_contained" = "xyes"; then
                    LDFLAGS="$LDFLAGS -Wl,-Bstatic -Wl,-lyaml-cpp -Wl,-Bdynamic"
             else
                     LIBS="-lyaml-cpp $LIBS"
             fi
             if test "x$enable_self_contained_uuid" = "xyes"; then
                     LDFLAGS="$LDFLAGS -Wl,-luuid"
             fi
             ],
    [AC_MSG_ERROR([cannot link with -lyaml-cpp])])
AC_LANG_POP([C++])


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC

# Output files
AC_CONFIG_HEADERS([include/config.h])

# One entry for each subdirectory
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 unit_tests/Makefile
                 examples/Makefile
                 examples/Helloworld/Makefile
                 examples/ToyScope/Makefile
                 keychain/Makefile])

AC_OUTPUT
