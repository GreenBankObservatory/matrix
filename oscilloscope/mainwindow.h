#include <qwidget.h>
#include <qboxlayout.h>
#include "samplingthread.h"

class Plot;
class Knob;
class WheelBox;
class QLabel;
class QPushButton;
using MyKnob = WheelBox;

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

public Q_SLOTS:
    void save_snapshot();

private:
    struct ControlTab
    {
        QWidget *tab;
        MyKnob *d_yoffsetKnob;
        MyKnob *d_yscaleKnob;
        WheelBox *d_fineoffsetWheel;
        QPushButton *d_centerY;
        QVBoxLayout *vlayout;
    };
    struct UtilsButtons
    {
        QWidget *tab;
        QVBoxLayout *vlayout;
        QPushButton *run_stop_button;
        QPushButton *snapshot_button;
        WheelBox *d_sample_interval_wheel;
    };

    ControlTab ch1;
    ControlTab ch2;
    UtilsButtons utilsButtons;

    Plot *d_plot;
    SamplingThread *ch1_sampler;
    SamplingThread *ch2_sampler;
};
