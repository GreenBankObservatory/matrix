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
    cerr << "usage: " << "oscilloscope" << " -str stream_alias -ch1 fieldname [-url keymaster_url] "  << endl;
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
    return 10.0;
}

void MainWindow::initialize(int argc, char **argv)
{

    sampler = new SamplingThread();
    sampler->setInterval(signalInterval());

    connect(this, SIGNAL(signalIntervalChanged(double)),
            sampler, SLOT(setInterval(double)));

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
        _exit(-1);
    }
    if (!sampler->set_display_field(ch1_fieldname))
    {
        cerr << "Error finding field " << ch1_fieldname << " in stream" << endl;
        _exit(-1);
    }

    d_plot->set_ch1_sampler(sampler);
//    d_plot->set_ch2_sampler(sampler_ch2);

    if (sampler)
    {
        sampler->start();
    }
//    if (sampler_ch2)
//    {
//        sampler_ch2->start();
//    }
}