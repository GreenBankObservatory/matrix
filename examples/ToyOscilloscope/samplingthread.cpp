#include "samplingthread.h"
#include "signaldata.h"
#include <qwt_math.h>
#include <math.h>
#include <stdio.h>
#include "Keymaster.h"
#include "DataInterface.h"
#include "DataSink.h"

#if QT_VERSION < 0x040600
#define qFastSin(x) ::sin(x)
#endif

SamplingThread::SamplingThread(QObject *parent):
    QwtSamplingThread(parent),
    d_frequency(5.0),
    d_amplitude(20.0),
    keymaster("tcp://localhost:42000"),
    input_signal_sink("tcp://localhost:42000")
{
    // fin = fopen("/tmp/data", "r");
    printf("connect to data\n");
    input_signal_sink.connect("accumulator","output_signal");
}

void SamplingThread::setFrequency(double frequency)
{
    d_frequency = frequency;
    printf("setting frequency to %lf\n", frequency);
    keymaster.put("components.signal_generator.frequency", frequency);
}

double SamplingThread::frequency() const
{
    return d_frequency;
}

void SamplingThread::setAmplitude(double amplitude)
{
    d_amplitude = amplitude;
    printf("setting amplitude to %lf\n", amplitude);
    keymaster.put("components.signal_generator.amplitude", amplitude);
}

double SamplingThread::amplitude() const
{
    return d_amplitude;
}

void SamplingThread::sample(double elapsed)
{
    if ( d_frequency > 0.0 )
    {
        const QPointF s(elapsed, value(elapsed));
        SignalData::instance().append(s);
    }
}

double SamplingThread::value(double timeStamp) 
{
    const double period = 1.0 / d_frequency;

    const double x = ::fmod(timeStamp, period);
    double v ; // = d_amplitude * qFastSin(x / period * 2 * M_PI);
    double dd;
    // fscanf(fin, "%lf\n", &v); 
    input_signal_sink.get(dd);
    // printf("%lf\n", dd);

    return dd;
}
