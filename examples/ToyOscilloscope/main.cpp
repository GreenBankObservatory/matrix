#include <qapplication.h>
#include "mainwindow.h"
#include "samplingthread.h"
#include <string>
#include <iostream>
using namespace std;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    MainWindow window;
    window.resize(800,400);

    SamplingThread samplingThread;
    samplingThread.setFrequency(window.frequency());
    samplingThread.setAmplitude(window.amplitude());
    samplingThread.setInterval(window.signalInterval());

    window.connect(&window, SIGNAL(frequencyChanged(double)),
        &samplingThread, SLOT(setFrequency(double)));
    window.connect(&window, SIGNAL(amplitudeChanged(double)),
        &samplingThread, SLOT(setAmplitude(double)));
    window.connect(&window, SIGNAL(signalIntervalChanged(double)),
        &samplingThread, SLOT(setInterval(double)));

    window.show();

    if (argc < 2)
    {
        cerr << "usage: " << argv[0] << " stream_alias"  << endl;
        exit(-1);
    }
    samplingThread.set_stream_alias(std::string(argv[1]));

    samplingThread.start();
    window.start();

    bool ok = app.exec(); 

    samplingThread.stop();
    samplingThread.wait(1000);

    return ok;
}
