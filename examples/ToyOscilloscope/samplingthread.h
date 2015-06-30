#include <qwt_sampling_thread.h>
#include <stdio.h>
#include "Keymaster.h"
#include "DataInterface.h"
#include "DataSink.h"

class SamplingThread: public QwtSamplingThread
{
    Q_OBJECT

public:
    SamplingThread(QObject *parent = NULL);

    double frequency() const;
    double amplitude() const;

public Q_SLOTS:
    void setAmplitude(double);
    void setFrequency(double);

protected:
    virtual void sample(double elapsed);

private:
    virtual double value(double timeStamp) ;

    double d_frequency;
    double d_amplitude;
    Keymaster keymaster;
    matrix::DataSink<double, matrix::select_only>  input_signal_sink;
    FILE *fin;
};
