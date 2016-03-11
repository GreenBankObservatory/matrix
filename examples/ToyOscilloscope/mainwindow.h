#include <qwidget.h>

class Plot;
class Knob;
class WheelBox;
class QLabel;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget * = NULL);

    void start();

    double amplitude() const;
    double frequency() const;
    double signalInterval() const;

Q_SIGNALS:
    void amplitudeChanged(double);
    void frequencyChanged(double);
    void signalIntervalChanged(double);

private:
    Knob *d_yoffsetKnob;
    Knob *d_yscaleKnob;
    WheelBox *d_timerWheel;
    WheelBox *d_intervalWheel;
    WheelBox *d_fineoffsetWheel;

    QLabel *d_infoLabel;

    Plot *d_plot;
};
