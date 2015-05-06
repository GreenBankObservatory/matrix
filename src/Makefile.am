# lib/Makefile.am
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

SUBDIRS = .

lib_LTLIBRARIES = libmatrix.la

headersdir = $(includedir)
headers_HEADERS = \
    Mutex.h \
    TCondition.h \
    Controller.h \
    Component.h \
    FiniteStateMachine.h \
    Keymaster.h \
    matrix_util.h \
    Semaphore.h \
    Thread.h \
    ThreadLock.h \
    Time.h \
    ZMQContext.h \
    netUtils.h \
    tsemfifo.h \
    yaml_util.h \
    zmq_util.h

libmatrix_la_SOURCES = \
    Mutex.cc  \
    Controller.cc \
    Component.cc \
    FiniteStateMachine.cc \
    Keymaster.cc \
    matrix_util.cc \
    Semaphore.cc \
    Time.cc \
    Thread.cc \
    ZMQContext.cc \
    netUtils.cc \
    yaml_util.cc \
    zmq_util.cc

libmatrix_la_CXXFLAGS = -pthread -std=c++0x -g 
libmatrix_la_CFLAGS =  -pthread -g
libmatrix_la_LDFLAGS = -lpthread

distclean-local:
	$(RM) -rf *.o *.a *.lo .deps .libs Makefile Makefile.in

dist-hook:
	$(RM) -rf *.o *.a *.lo .deps .libs Makefile Makefile.in
