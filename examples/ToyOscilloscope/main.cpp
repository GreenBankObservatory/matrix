#include <qapplication.h>
#include "mainwindow.h"
#include "samplingthread.h"
#include <string>
#include <iostream>
using namespace std;

void usage()
{
    cerr << "usage: " << "oscilloscope" << " -str stream_alias [-url keymaster_url] "  << endl;
    cerr << "\t (keymaster url defaults to tcp://localhost:42000)" << endl;
}

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

    string stream_alias;
    string key_url = "tcp://localhost:42000";
    for (auto i=1; i<argc; ++i)
    {
        string arg = argv[i];
        if (arg == "-str")
        {
            ++i;
            stream_alias = argv[i];
        }
        else if (arg == "-url")
        {
            ++i;
            key_url = argv[i];
        }
        else
        {
            cerr << "don't recognize option: " << arg << endl;
            usage();
            exit(-1);
        }
    }
    if (stream_alias.size() < 1)
    {
        cerr << "a stream must be provided" << endl;
        usage();
        exit(-1);
    }
    samplingThread.set_keymaster_url(key_url);
    if (!samplingThread.set_stream_alias(stream_alias))
    {
        cerr << "Error getting stream" << endl;
        exit(-1);
    }

    samplingThread.start();
    window.start();

    bool ok = app.exec(); 

    samplingThread.stop();
    samplingThread.wait(1000);

    return ok;
}
