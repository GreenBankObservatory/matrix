# unit_test/Makefile.am
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

noinst_PROGRAMS = matrix_unittest

matrix_unittest_SOURCES = \
	ControllerTest.cc \
	DataInterfaceTest.cc \
	StateTransitionTest.cc \
	TimeTest.cc \
	keymaster_test.cc \
	matrix_unittest.cc \
	utility_test.cc

matrix_unittest_CXXFLAGS = -std=c++0x -I/home/gbt1/RH564/cppunit-1.12.1/include -I../src -O0 -g
#matrix_unittest_LDADD = -L../src/.libs -lmatrix /home/gbt1/RH564//cppunit-1.12.1/i64-linux/lib/libcppunit.a -lrt
matrix_unittest_LDADD = ../src/.libs/libmatrix.a /home/gbt1/RH564/cppunit-1.12.1/i64-linux/lib/libcppunit.a -lrt -lboost_regex

distclean-local:
	$(RM) -rf *.o *.a *.lo .deps .libs Makefile Makefile.in

dist-hook:
	$(RM) -rf *.o *.a *.lo .deps .libs Makefile Makefile.in
