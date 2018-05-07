
# C++11 required.
QMAKE_CXXFLAGS += -std=c++11 -Wno-comment -Wno-unused-variable -Wno-sign-compare
# More locally specific stuff edit as required
LIBS += -lmatrix -lyaml-cpp -lzmq -lboost_regex -lrt -lqwt

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
