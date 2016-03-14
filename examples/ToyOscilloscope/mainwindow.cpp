#include "mainwindow.h"
#include "plot.h"
#include "knob.h"
#include "wheelbox.h"
#include <iostream>

#include <qwt_scale_engine.h>
#include <qlabel.h>
#include <qlayout.h>

using namespace std;

static void usage()
{
    cerr << "usage: " << "oscilloscope" << " -str stream_alias -ch1 fieldname [-url keymaster_url] "  << endl;
    cerr << "\t (keymaster url defaults to tcp://localhost:42000)" << endl;
}

MainWindow::MainWindow(QWidget *parent):
    QWidget(parent)
{
    const double intervalLength = 10.0; // seconds

    d_plot = new Plot(this);
    d_plot->setIntervalLength(intervalLength);

    d_yscaleKnob = new Knob("Y-Scale div", 0.01, 50.0, this);
    d_yscaleKnob->setValue(1.0);
    
    d_yoffsetKnob = new Knob("Y-Offset", -90.0, 90.0, this);
    d_yoffsetKnob->setValue(0.0);

    d_intervalWheel = new WheelBox("Displayed [s]", 1.0, 100.0, 1.0, this);
    d_intervalWheel->setValue(intervalLength);
    d_fineoffsetWheel = new WheelBox("Fine Offset", -1.0, 1.0, 0.1, this);
    d_fineoffsetWheel->setValue(0.0);

    d_infoLabel = new QLabel(tr("<i>CH1</i>"));
    d_infoLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    d_infoLabel->setAlignment(Qt::AlignCenter);

    d_timerWheel = new WheelBox("Sample Interval [ms]", 0.0, 20.0, 0.1, this);
    d_timerWheel->setValue(10.0);

    QVBoxLayout* vLayout1 = new QVBoxLayout();
    vLayout1->addWidget(d_intervalWheel);
    // vLayout1->addWidget(d_timerWheel);
    vLayout1->addStretch(10);
    vLayout1->addWidget(d_yscaleKnob);
    vLayout1->addWidget(d_yoffsetKnob);
    vLayout1->addWidget(d_fineoffsetWheel);
    vLayout1->addWidget(d_infoLabel);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(d_plot, 10);
    layout->addLayout(vLayout1);

    connect(d_yscaleKnob, SIGNAL(valueChanged(double)),
            SIGNAL(amplitudeChanged(double)));
    connect(d_yoffsetKnob, SIGNAL(valueChanged(double)),
            SIGNAL(frequencyChanged(double)));
    connect(d_timerWheel, SIGNAL(valueChanged(double)),
        SIGNAL(signalIntervalChanged(double)));

    connect(d_intervalWheel, SIGNAL(valueChanged(double)),
        d_plot, SLOT(setIntervalLength(double)) );
    connect(d_yoffsetKnob, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setYOffset(double)) );
    connect(d_yscaleKnob, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setYScale(double)) );
    connect(d_fineoffsetWheel, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setFineOffset(double)) );
}

void MainWindow::start()
{
    d_plot->start();
    sampler->start();
}

void MainWindow::stop()
{
    sampler->stop();
}

void MainWindow::wait(int t_ms)
{
    sampler->wait(t_ms);
}

double MainWindow::yoffset() const
{
    return d_yoffsetKnob->value();
}

double MainWindow::yscale() const
{
    return d_yscaleKnob->value();
}

double MainWindow::signalInterval() const
{
    return d_timerWheel->value();
}

void MainWindow::initialize(int argc, char **argv)
{
    SamplingThread samplingThread;
//    samplingThread.setFrequency(window.yoffset());
//    samplingThread.setAmplitude(window.yscale());
    sampler.reset( new SamplingThread() );
    sampler->setInterval(signalInterval());

//    window.connect(&window, SIGNAL(frequencyChanged(double)),
//        &samplingThread, SLOT(setFrequency(double)));
//    window.connect(&window, SIGNAL(amplitudeChanged(double)),
//        &samplingThread, SLOT(setAmplitude(double)));
    connect(this, SIGNAL(signalIntervalChanged(double)),
             sampler.get(), SLOT(setInterval(double)));

    string stream_alias;
    string key_url = "tcp://localhost:42000";
    string ch1_fieldname;

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
        else if (arg == "-ch1")
        {
            ++i;
            ch1_fieldname = argv[i];
        }
        else
        {
            cerr << " I don't recognize option: " << arg << endl;
            usage();
            exit(-1);
        }
    }
    if (stream_alias.size() < 1)
    {
        cerr << "a valid stream must be provided" << endl;
        usage();
        exit(-1);
    }
    sampler->set_keymaster_url(key_url);
    if (!sampler->set_stream_alias(stream_alias))
    {
        cerr << "Error getting stream" << endl;
        exit(-1);
    }
    if (!sampler->set_display_field(ch1_fieldname))
    {
        cerr << "Error finding field " << ch1_fieldname << " in stream" << endl;
        _exit(-1);
    }

    sampler->start();
}