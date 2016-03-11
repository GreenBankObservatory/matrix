#include <qwt_sampling_thread.h>
#include <stdio.h>
#include "Keymaster.h"
#include "DataInterface.h"
#include "DataSink.h"
#include "TCondition.h"
#include "Thread.h"

class SamplingThread: public QwtSamplingThread
{
    Q_OBJECT

public:
    SamplingThread(QObject *parent = NULL);

    double frequency() const;
    double amplitude() const;
    bool set_stream_alias(std::string stream);
    bool set_keymaster_url(std::string key);
    bool set_display_field(std::string field);

public Q_SLOTS:
    void setAmplitude(double);
    void setFrequency(double);

protected:
    virtual void sample(double elapsed);

private:
    virtual double value(double timeStamp) ;
    void sink_reader_thread();

    double d_frequency;
    double d_amplitude;

    std::unique_ptr<Keymaster> keymaster;
    std::unique_ptr<matrix::DataSink<matrix::GenericBuffer> > input_signal_sink;
    std::unique_ptr<matrix::data_description> ddesc;
    matrix::GenericBuffer gbuffer;
    FILE *fin;
    Thread<SamplingThread> sink_thread;
    TCondition<bool> sink_thread_started;
    std::string ch1_fieldname;
    double d_last_value;
};
