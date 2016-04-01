#include <qwidget.h>
#include <qboxlayout.h>
#include "samplingthread.h"

class Plot;
class Knob;
class WheelBox;
class QLabel;
class QPushButton;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget * = NULL);

    void initialize(int argc, char **argv);

    void start();
    void stop();
    void wait(int t_ms);

    double yscale() const;
    double yoffset() const;
    double signalInterval() const;

Q_SIGNALS:
    void amplitudeChanged(double);
    void frequencyChanged(double);
    void signalIntervalChanged(double);

private:
    struct ControlTab
    {
        QWidget *tab;
        Knob *d_yoffsetKnob;
        Knob *d_yscaleKnob;
        WheelBox *d_sample_interval_wheel;
        WheelBox *d_intervalWheel;
        WheelBox *d_fineoffsetWheel;

        QPushButton *d_centerY;
        QPushButton *run_stop_button;
        QVBoxLayout *vlayout;
    };

    ControlTab ch1;
    ControlTab ch2;

    Plot *d_plot;
    SamplingThread *ch1_sampler;
    SamplingThread *ch2_sampler;
};
