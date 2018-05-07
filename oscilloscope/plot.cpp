#include "plot.h"
#include "curvedata.h"
#include "signaldata.h"
#include <qwt/qwt_plot_grid.h>
#include <qwt/qwt_plot_layout.h>
#include <qwt/qwt_plot_canvas.h>
#include <qwt/qwt_plot_marker.h>
#include <qwt/qwt_plot_curve.h>
#include <qwt/qwt_plot_directpainter.h>
#include <qwt/qwt_curve_fitter.h>
#include <qwt/qwt_painter.h>
#include <qevent.h>

#include <iostream>

using namespace std;

Plot::Plot(QWidget *parent):
    QwtPlot(parent),
    d_ch1(nullptr),
    d_ch2(nullptr),
    d_painted_ch1(0),
    d_painted_ch2(0),
    ch1_sampler(),
    ch2_sampler(),
    d_interval(0.0, 10.0),
    d_timerId(-1),
    paused(false)
{
    d_directPainter = new QwtPlotDirectPainter();

    setAutoReplot(false);

    // The backing store is important, when working with widget
    // overlays ( f.e rubberbands for zooming ). 
    // Here we don't have them and the internal 
    // backing store of QWidget is good enough.
    QwtPlotCanvas *pltcanvas = (QwtPlotCanvas *)canvas();

    pltcanvas->setPaintAttribute(QwtPlotCanvas::BackingStore, false);


#if defined(Q_WS_X11)
    // Even if not recommended by TrollTech, Qt::WA_PaintOutsidePaintEvent
    // works on X11. This has a nice effect on the performance.
    
    canvas()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);

    // Disabling the backing store of Qt improves the performance
    // for the direct painter even more, but the canvas becomes
    // a native window of the window system, receiving paint events
    // for resize and expose operations. Those might be expensive
    // when there are many points and the backing store of
    // the canvas is disabled. So in this application
    // we better don't both backing stores.

    if ( pltcanvas->testPaintAttribute( QwtPlotCanvas::BackingStore ) )
    {
        pltcanvas->setAttribute(Qt::WA_PaintOnScreen, true);
        pltcanvas->setAttribute(Qt::WA_NoSystemBackground, true);
    }

#endif

    initGradient();

    plotLayout()->setAlignCanvasToScales(true);

    setAxisTitle(QwtPlot::xBottom, "Time [s]");
    setAxisScale(QwtPlot::xBottom, d_interval.minValue(), d_interval.maxValue()); 
    d_scale = 50;
    d_yoffset = 0.0;
    d_fine_offset = 0.0;
    double top, bottom;
    top =    d_yoffset + d_scale;
    bottom = d_yoffset - d_scale;
    setAxisScale(QwtPlot::yLeft, bottom, top);

    d_y2scale = 50.0;
    d_y2offset = 0.0;
    top =    d_y2offset + d_y2scale;
    bottom = d_y2offset - d_y2scale;
    enableAxis(QwtPlot::yRight, true);
    setAxisScale(QwtPlot::yRight, bottom, top);

    QwtPlotGrid *grid = new QwtPlotGrid();
    grid->setPen(QPen(Qt::gray, 0.0, Qt::DotLine));
    grid->enableX(true);
    grid->enableXMin(true);
    grid->enableY(true);
    grid->enableYMin(false);
    grid->attach(this);

    d_origin = new QwtPlotMarker();
    d_origin->setLineStyle(QwtPlotMarker::Cross);
    d_origin->setValue(d_interval.minValue() + d_interval.width() / 2.0, 0.0);
    d_origin->setLinePen(QPen(Qt::gray, 0.0, Qt::DashLine));
    d_origin->attach(this);

    cerr << "Plot window id = " << QWidget::winId() << endl;

}

Plot::~Plot()
{
    delete d_directPainter;
}

void Plot::initGradient()
{
    QPalette pal = canvas()->palette();

#if QT_VERSION >= 0x040400
    QLinearGradient gradient( 0.0, 0.0, 1.0, 0.0 );
    gradient.setCoordinateMode( QGradient::StretchToDeviceMode );
    gradient.setColorAt(0.0, QColor( 0, 49, 110 ) );
    gradient.setColorAt(1.0, QColor( 0, 87, 174 ) );

    pal.setBrush(QPalette::Window, QBrush(gradient));
#else
    pal.setBrush(QPalette::Window, QBrush( color ));
#endif

    canvas()->setPalette(pal);
}

void Plot::start()
{
    d_clock.start();
    d_timerId = startTimer(10);
    // we always start ch1
    ch1_sampler->start();

    if (ch2_sampler)
    {
        ch2_sampler->start();
    }
}

void Plot::stop()
{
    // we always start ch1
    ch1_sampler->stop();

    if (ch2_sampler)
    {
        ch2_sampler->stop();
    }
}

void Plot::replot()
{
    CurveData *ch1_data = (CurveData *) d_ch1->data();
    CurveData *ch2_data = nullptr;

    ch1_data->values().lock();

    if (ch2_sampler)
    {
        ch2_data = (CurveData *) d_ch2->data();
        ch2_data->values().lock();
    }
    QwtPlot::replot();
    d_painted_ch1 = ch1_data->size();

    if (ch2_sampler)
    {
        d_painted_ch2 = ch2_data->size();
        ch2_data->values().unlock();
    }
    ch1_data->values().unlock();
}

void Plot::setIntervalLength(double interval)
{
    if ( interval > 0.0 && interval != d_interval.width() )
    {
        d_interval.setMaxValue(d_interval.minValue() + interval);
        setAxisScale(QwtPlot::xBottom, 
            d_interval.minValue(), d_interval.maxValue());

        replot();
    }
}

void Plot::setYScale(double scale)
{
    d_scale = fabs(scale);
    double top =    d_yoffset + d_scale; // + (d_fine_offset * d_scale);
    double bottom = d_yoffset - d_scale; // + (d_fine_offset * d_scale);

    setAxisScale(QwtPlot::yLeft, bottom, top);

    replot();
}
void Plot::setYOffset(double offset)
{
    d_yoffset = offset;
    double top =    d_yoffset + d_scale; // + (d_fine_offset * d_scale);
    double bottom = d_yoffset - d_scale; // + (d_fine_offset * d_scale);

    setAxisScale(QwtPlot::yLeft, bottom, top);

    replot();
}
void Plot::setY2Scale(double scale)
{
    d_y2scale = fabs(scale);
    double top =    d_y2offset + d_y2scale; // + (d_fine_offset * d_scale);
    double bottom = d_y2offset - d_y2scale; // + (d_fine_offset * d_scale);

    setAxisScale(QwtPlot::yRight, bottom, top);

    replot();
}
void Plot::setY2Offset(double offset)
{
    d_y2offset = offset;
    double top =    d_y2offset + d_y2scale; // + (d_fine_offset * d_y2scale);
    double bottom = d_y2offset - d_y2scale; // + (d_fine_offset * d_y2scale);

    setAxisScale(QwtPlot::yRight, bottom, top);

    replot();
}

void Plot::setFineOffset(double foffset)
{
    d_fine_offset = foffset;
    double top =    d_yoffset + d_scale; // + (d_fine_offset * d_scale);
    double bottom = d_yoffset - d_scale; // + (d_fine_offset * d_scale);

    setAxisScale(QwtPlot::yLeft, bottom, top);

    replot();
}

void Plot::centerY()
{
    CurveData *ch1_data = (CurveData *) d_ch1->data();
    ch1_data->values().lock();
    int numPoints = ch1_data->size();
    QRectF br = qwtBoundingRect(*ch1_data, d_painted_ch1 - 1, numPoints-1);
    ch1_data->values().unlock();

    double x, y, w, h;
    br.getRect(&x, &y, &w, &h);
    setYOffset(y + h/2.);
    offsetYChanged(d_yoffset);
    // printf("center Y: %f %f %f %f\n", x, y, w, h);
}

void Plot::zoom_in_Y()
{
    setYScale(fabs(d_scale/2.0));
    scaleYChanged(d_scale);
}

void Plot::zoom_out_Y()
{
    setYScale(fabs(d_scale * 2.0));
    scaleYChanged(d_scale);
}

void Plot::centerY2()
{
    CurveData *ch2_data = (CurveData *) d_ch2->data();
    ch2_data->values().lock();
    int numPoints = ch2_data->size();
    QRectF br = qwtBoundingRect(*ch2_data, d_painted_ch2 - 1, numPoints-1);
    ch2_data->values().unlock();

    double x, y, w, h;
    br.getRect(&x, &y, &w, &h);
    setY2Offset(y + h/2.);
    offsetY2Changed(d_y2offset);
}

void Plot::zoom_in_Y2()
{
    setY2Scale(fabs(d_y2scale / 2.0));
    scaleY2Changed(d_y2scale);
}

void Plot::zoom_out_Y2()
{
    setY2Scale(fabs(d_y2scale * 2.0));
    scaleY2Changed(d_y2scale);
}

void Plot::updateCurve()
{
    CurveData *ch1_data = (CurveData *) d_ch1->data();
    CurveData *ch2_data = nullptr;

    ch1_data->values().lock();

    int numPoints = ch1_data->size();

    if (numPoints > d_painted_ch1)
    {
        const bool doClip = !canvas()->testAttribute( Qt::WA_PaintOnScreen );
        if ( doClip )
        {
            /*
                Depending on the platform setting a clip might be an important
                performance issue. F.e. for Qt Embedded this reduces the
                part of the backing store that has to be copied out - maybe
                to an unaccelerated frame buffer device.
            */
            const QwtScaleMap xMap = canvasMap( d_ch1->xAxis() );
            const QwtScaleMap yMap = canvasMap( d_ch1->yAxis() );

            QRectF br = qwtBoundingRect(*ch1_data, d_painted_ch1 - 1, numPoints-1);

            const QRect clipRect = QwtScaleMap::transform( xMap, yMap, br ).toRect();
            d_directPainter->setClipRegion( clipRect );
        }
        d_directPainter->drawSeries(d_ch1, d_painted_ch1 - 1, numPoints-1);
        d_painted_ch1 = numPoints;
    }

    if (ch2_sampler)
    {
        ch2_data = (CurveData *) d_ch2->data();
        ch2_data->values().lock();
        numPoints = ch2_data->size();
        if (numPoints > d_painted_ch2)
        {
            const bool doClip = !canvas()->testAttribute(Qt::WA_PaintOnScreen);
            if (doClip)
            {
                /*
                    Depending on the platform setting a clip might be an important
                    performance issue. F.e. for Qt Embedded this reduces the
                    part of the backing store that has to be copied out - maybe
                    to an unaccelerated frame buffer device.
                */
                const QwtScaleMap xMap = canvasMap(d_ch2->xAxis());
                const QwtScaleMap yMap = canvasMap(d_ch2->yAxis());

                QRectF br = qwtBoundingRect(*ch2_data, d_painted_ch2 - 1, numPoints - 1);

                const QRect clipRect = QwtScaleMap::transform(xMap, yMap, br).toRect();
                d_directPainter->setClipRegion(clipRect);
            }
            d_directPainter->drawSeries(d_ch2, d_painted_ch2 - 1, numPoints - 1);
            d_painted_ch2 = numPoints;
        }
        ch2_data->values().unlock();
    }

    ch1_data->values().unlock();
}

void Plot::incrementInterval()
{
    d_interval = QwtInterval(d_interval.maxValue(),
        d_interval.maxValue() + d_interval.width());

    CurveData *data = (CurveData *) d_ch1->data();
    data->values().clearStaleValues(d_interval.minValue());

    // To avoid, that the grid is jumping, we disable 
    // the autocalculation of the ticks and shift them
    // manually instead.

    QwtScaleDiv scaleDiv = axisScaleDiv(QwtPlot::xBottom);
    scaleDiv.setInterval(d_interval);

    for ( int i = 0; i < QwtScaleDiv::NTickTypes; i++ )
    {
        QList<double> ticks = scaleDiv.ticks(i);
        for ( int j = 0; j < ticks.size(); j++ )
            ticks[j] += d_interval.width();
        scaleDiv.setTicks(i, ticks);
    }
    setAxisScaleDiv(QwtPlot::xBottom, scaleDiv);

    d_origin->setValue(d_interval.minValue() + d_interval.width() / 2.0, 0.0);

    d_painted_ch1 = 0;

    if (ch2_sampler)
    {
        data = (CurveData *) d_ch2->data();
        data->values().clearStaleValues(d_interval.minValue());
        d_painted_ch2 = 0;
    }

    replot();
}

void Plot::timerEvent(QTimerEvent *event)
{
    if ( event->timerId() == d_timerId )
    {
        updateCurve();

        const double elapsed = d_clock.elapsed() / 1000.0;
        if ( elapsed > d_interval.maxValue() && !paused)
            incrementInterval();

        return;
    }

    QwtPlot::timerEvent(event);
}

void Plot::resizeEvent(QResizeEvent *event)
{
    d_directPainter->reset();
    QwtPlot::resizeEvent(event);
}

void Plot::showEvent( QShowEvent * )
{
    replot();
}

void Plot::run_stop_click()
{
    if (!paused)
    {
        cerr << "Stop" << endl;
        paused = true;
    }
    else
    {
        paused = false;
        cerr << "Run" << endl;
    }
    ch1_sampler->pause(paused);
    if (ch2_sampler)
    {
        ch2_sampler->pause(paused);
    }
}

// Note Plot takes ownership of SamplingThread
void Plot::set_ch1_sampler(SamplingThread *s)
{
    CurveData *cd = new CurveData();
    ch1_sampler.reset(s);
    d_ch1 = new QwtPlotCurve();
    d_ch1->setStyle(QwtPlotCurve::Lines);
    d_ch1->setPen(QPen(Qt::green, 0.0));

    d_ch1->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    d_ch1->setPaintAttribute(QwtPlotCurve::ClipPolygons, false);
    d_ch1->setData(cd);
    d_ch1->attach(this);
    s->setData(cd);
}

// Note Plot takes ownership of SamplingThread
void Plot::set_ch2_sampler(SamplingThread *s)
{
    CurveData *cd = new CurveData();
    ch2_sampler.reset(s);
    d_ch2 = new QwtPlotCurve();
    d_ch2->setStyle(QwtPlotCurve::Lines);
    d_ch2->setPen(QPen(Qt::red, 0.0));

    d_ch2->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    d_ch2->setPaintAttribute(QwtPlotCurve::ClipPolygons, false);
    d_ch2->setData(cd);
    d_ch2->setYAxis(QwtPlot::yRight);
    d_ch2->attach(this);
    s->setData(cd);
}
