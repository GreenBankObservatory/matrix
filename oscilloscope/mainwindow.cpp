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
    ch1.tab = new QWidget();
    ch2.tab = new QWidget();

    ch1.tab->setObjectName("tCH1");
    tabWidget->addTab(ch1.tab, "CH1");

    ch2.tab->setObjectName("tCH2");
    tabWidget->addTab(ch2.tab, "CH2");

    ch1.d_yscaleKnob = new MyKnob("Y-Scale div", 0.0, 50.0, 1.0, ch1.tab);
    ch1.d_yscaleKnob->setValue(1.0);

    ch1.d_yoffsetKnob = new MyKnob("Y-Offset", -90.0, 90.0, 1.0, ch1.tab);
    ch1.d_yoffsetKnob->setValue(0.0);

    ch1.d_intervalWheel = new WheelBox("Displayed [s]", 1.0, 100.0, 1.0, ch1.tab);
    ch1.d_intervalWheel->setValue(intervalLength);
    ch1.d_fineoffsetWheel = new WheelBox("Fine Offset", -1.0, 1.0, 0.1, ch1.tab);
    ch1.d_fineoffsetWheel->setValue(0.0);

    ch1.d_centerY = new QPushButton("Center-Y", ch1.tab);
    ch1.run_stop_button = new QPushButton("Run/Stop", this);

    ch1.vlayout = new QVBoxLayout(ch1.tab);
    ch1.vlayout->addWidget(ch1.d_intervalWheel);
    ch1.vlayout->addStretch(10);
    ch1.vlayout->addWidget(ch1.d_yscaleKnob);
    ch1.vlayout->addWidget(ch1.d_yoffsetKnob);
    ch1.vlayout->addWidget(ch1.d_fineoffsetWheel);
    ch1.vlayout->addWidget(ch1.d_centerY);
    ch1.vlayout->addWidget(ch1.run_stop_button);


    ch2.d_yscaleKnob = new MyKnob("Y2-Scale div", 0.0, 50.0, 1.0, ch2.tab);
    ch2.d_yscaleKnob->setValue(1.0);

    ch2.d_yoffsetKnob = new MyKnob("Y2-Offset", -90.0, 90.0, 1.0, ch2.tab);
    ch2.d_yoffsetKnob->setValue(0.0);

    ch2.d_intervalWheel = new WheelBox("Displayed [s]", 1.0, 100.0, 1.0, ch2.tab);
    ch2.d_intervalWheel->setValue(intervalLength);
    ch2.d_fineoffsetWheel = new WheelBox("Fine Offset", -1.0, 1.0, 0.1, ch2.tab);
    ch2.d_fineoffsetWheel->setValue(0.0);

    ch2.d_centerY = new QPushButton("Center-Y", ch2.tab);
    ch2.run_stop_button = new QPushButton("Run/Stop", this);

    ch2.vlayout = new QVBoxLayout(ch2.tab);
    ch2.vlayout->addWidget(ch2.d_intervalWheel);
    ch2.vlayout->addStretch(10);
    ch2.vlayout->addWidget(ch2.d_yscaleKnob);
    ch2.vlayout->addWidget(ch2.d_yoffsetKnob);
    ch2.vlayout->addWidget(ch2.d_fineoffsetWheel);
    ch2.vlayout->addWidget(ch2.d_centerY);
    ch2.vlayout->addWidget(ch2.run_stop_button);




    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(d_plot, 10);
    layout->addWidget(tabWidget);
    // layout->addLayout(vLayout1);


    connect(ch1.d_yoffsetKnob, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setYOffset(double)) );
    connect(ch1.d_yscaleKnob, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setYScale(double)) );
    connect(ch1.d_fineoffsetWheel, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setFineOffset(double)) );

    connect(ch2.d_yoffsetKnob, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setY2Offset(double)) );
    connect(ch2.d_yscaleKnob, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setY2Scale(double)) );

    connect(ch1.d_centerY, SIGNAL(clicked(void)),
            d_plot, SLOT(centerY(void)));
    connect(ch2.d_centerY, SIGNAL(clicked(void)),
            d_plot, SLOT(centerY2(void)));

    // Should only have one of these
    connect(ch1.run_stop_button, SIGNAL(clicked(void)),
            d_plot, SLOT(run_stop_click(void)));
    connect(ch1.d_intervalWheel, SIGNAL(valueChanged(double)),
            d_plot, SLOT(setIntervalLength(double)) );
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
    return ch1.d_yoffsetKnob->value();
}

double MainWindow::yscale() const
{
    return ch1.d_yscaleKnob->value();
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