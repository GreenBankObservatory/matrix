################################################################
# Qwt Widget Library
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
################################################################

include( $${PWD}/examples.pri )

# Extra include paths for the matrix libs and our non-standard
# YAML and 0MQ installations.
THIRDPARTYLIBS=/home/gbt1/lib_devel
INCLUDEPATH += ../src
INCLUDEPATH += /home/gbt1/lib_devel/include

# C++11 required.
QMAKE_CXXFLAGS += -std=c++11 -Wno-comment -Wno-unused-variable -Wno-sign-compare
# More locally specific stuff edit as required
LIBS += -L/home/gbt1/lib_devel/lib -lmatrix -lyaml-cpp -lzmq -lboost_regex -lrt

TARGET   = oscilloscope

HEADERS = \
    signaldata.h \
    plot.h \
    knob.h \
    wheelbox.h \
    samplingthread.h \
    curvedata.h \
    mainwindow.h 

SOURCES = \
    signaldata.cpp \
    plot.cpp \
    knob.cpp \
    wheelbox.cpp \
    samplingthread.cpp \
    curvedata.cpp \
    mainwindow.cpp \
    main.cpp
