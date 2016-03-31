#include "mainwindow.h"
#include "plot.h"
#include "knob.h"
#include "wheelbox.h"
#include <iostream>

#include <qwt_scale_engine.h>
#include <qlabel.h>
#include <qlayout.h>
#include <QPushButton>
#include <QTabWidget>

using namespace std;

static void usage()
{
    cerr << "usage: " << "oscilloscope" << " -ch1 stream.field [ -ch2 stream.field ][ -url keymaster_url ] "  << endl;
    cerr << "\t (keymaster url defaults to tcp://localhost:42000)" << endl;
}

MainWindow::MainWindow(QWidget *parent):
    QWidget(parent)
{
    const double intervalLength = 10.0; // seconds

    d_plot = new Plot(this);
    d_plot->setIntervalLength(intervalLength);

    QTabWidget *tabWidget = new QTabWidget();
    QWidget *tab1 = new QWidget();
    QWidget *tab2 = new QWidget();

    tab1->setObjectName("tCH1");
    tabWidget->addTab(tab1, "CH1");

    tab2->setObjectName("tCH2");
    tabWidget->addTab(tab2, "CH2");

    d_yscaleKnob = new Knob("Y-Scale div", 0.01, 50.0, tab1);
    d_yscaleKnob->setValue(1.0);

    d_yoffsetKnob = new Knob("Y-Offset", -90.0, 90.0, tab1);
    d_yoffsetKnob->setValue(0.0);

    d_intervalWheel = new WheelBox("Displayed [s]", 1.0, 100.0, 1.0, tab1);
    d_intervalWheel->setValue(intervalLength);
    d_fineoffsetWheel = new WheelBox("Fine Offset", -1.0, 1.0, 0.1, tab1);
    d_fineoffsetWheel->setValue(0.0);

    d_centerY = new QPushButton("Center-Y", tab1);
    //d_centerY->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    //d_centerY->setAlignment(Qt::AlignCenter);

    run_stop_button = new QPushButton("Run/Stop", this);
    QVBoxLayout* vLayout1 = new QVBoxLayout(tab1);
    vLayout1->addWidget(d_intervalWheel);
    vLayout1->addStretch(10);
    vLayout1->addWidget(d_yscaleKnob);
    vLayout1->addWidget(d_yoffsetKnob);
    vLayout1->addWidget(d_fineoffsetWheel);
    vLayout1->addWidget(d_centerY);
    vLayout1->addWidget(run_stop_button);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(d_plot, 10);
    layout->addWidget(tabWidget);
    // layout->addLayout(vLayout1);

    connect(d_intervalWheel, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setIntervalLength(double)) );
    connect(d_yoffsetKnob, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setYOffset(double)) );
    connect(d_yscaleKnob, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setYScale(double)) );
    connect(d_fineoffsetWheel, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setFineOffset(double)) );
    connect(run_stop_button, SIGNAL(clicked(void)),
            d_plot, SLOT(run_stop_click(void)));
}

void MainWindow::start()
{
    d_plot->start();
}

void MainWindow::stop()
{
    d_plot->stop();
}

void MainWindow::wait(int t_ms)
{
    ch1_sampler->wait(t_ms);
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
    return 10.0;
}

void MainWindow::initialize(int argc, char **argv)
{

    ch1_sampler = new SamplingThread();
    ch2_sampler = nullptr;

    ch1_sampler->setInterval(signalInterval());

    string stream_alias;
    string key_url = "tcp://localhost:42000";
    string ch1_stream;
    string ch2_stream;
    string ch1_fieldname;
    string ch2_fieldname;
    string str_ch;
    bool use_ch2 = false;

    // Use the notation stream.channel instead of separate args.
    // So something like -ch1 az_encoder.position -ch2 el_encoder.position ...
    for (auto i=1; i<argc; ++i)
    {
        string arg = argv[i];

        if (arg == "-url")
        {
            ++i;
            key_url = argv[i];
        }
        else if (arg == "-ch1")
        {
            ++i;
            str_ch = argv[i];
            size_t dotidx = str_ch.find_first_of('.');
            ch1_stream = str_ch.substr(0, dotidx);
            ch1_fieldname = str_ch.substr(dotidx+1);
        }
        else if(arg == "-ch2")
        {
            ++i;
            str_ch = argv[i];
            size_t dotidx = str_ch.find_first_of('.');
            ch2_stream = str_ch.substr(0, dotidx);
            ch2_fieldname = str_ch.substr(dotidx+1);
            use_ch2 = true;
        }
        else
        {
            cerr << " I don't recognize option: " << arg << endl;
            usage();
            exit(-1);
        }
    }
    if (ch1_stream.size() < 1)
    {
        cerr << "a valid stream must be provided" << endl;
        usage();
        exit(-1);
    }
    ch1_sampler->set_keymaster_url(key_url);
    if (!ch1_sampler->set_stream_alias(ch1_stream))
    {
        cerr << "Error getting stream" << endl;
        _exit(-1);
    }
    if (!ch1_sampler->set_display_field(ch1_fieldname))
    {
        cerr << "Error finding field " << ch1_fieldname << " in stream" << endl;
        _exit(-1);
    }
    d_plot->set_ch1_sampler(ch1_sampler);

    if (use_ch2)
    {
        ch2_sampler = new SamplingThread();
        ch2_sampler->setInterval(signalInterval());
        ch2_sampler->set_keymaster_url(key_url);
        ch2_sampler->set_stream_alias(ch2_stream);

        if (!ch2_sampler->set_display_field(ch2_fieldname))
        {
            cerr << "Error finding ch2 field " << ch2_fieldname << " in stream" << endl;
            _exit(-1);
        }
        d_plot->set_ch2_sampler(ch2_sampler);
    }

    if (ch1_sampler)
    {
        connect(this, SIGNAL(signalIntervalChanged(double)),
                ch1_sampler, SLOT(setInterval(double)));
        ch1_sampler->start();
    }
    if (use_ch2)
    {
        connect(this, SIGNAL(signalIntervalChanged(double)),
                ch2_sampler, SLOT(setInterval(double)));
        ch2_sampler->start();
    }
}