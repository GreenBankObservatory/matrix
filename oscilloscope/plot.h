#include <qwt_plot.h>
#include <qwt_interval.h>
#include <qwt_system_clock.h>
#include "samplingthread.h"

class QwtPlotCurve;
class QwtPlotMarker;
class QwtPlotDirectPainter;

class Plot: public QwtPlot
{
    Q_OBJECT

public:
    Plot(QWidget * = NULL);
    virtual ~Plot();

    void start();
    void stop();
    virtual void replot();

public Q_SLOTS:
    void setIntervalLength(double);
    void setYScale(double);
    void setYOffset(double);
    void setY2Scale(double);
    void setY2Offset(double);
    void setFineOffset(double);
    void run_stop_click();
    void set_ch1_sampler(SamplingThread *);
    void set_ch2_sampler(SamplingThread *);

protected:
    virtual void showEvent(QShowEvent *);
    virtual void resizeEvent(QResizeEvent *);
    virtual void timerEvent(QTimerEvent *);

private:
    void initGradient();

    void updateCurve();
    void incrementInterval();

    QwtPlotMarker *d_origin;
    QwtPlotCurve *d_ch1;
    QwtPlotCurve *d_ch2;
    int d_painted_ch1;
    int d_painted_ch2;
    std::unique_ptr<SamplingThread> ch1_sampler;
    std::unique_ptr<SamplingThread> ch2_sampler;

    QwtPlotDirectPainter *d_directPainter;

    QwtInterval d_interval;
    int d_timerId;

    QwtSystemClock d_clock;
    double d_scale;
    double d_yoffset;
    double d_y2scale;
    double d_y2offset;
    double d_fine_offset;
    bool paused;
};
