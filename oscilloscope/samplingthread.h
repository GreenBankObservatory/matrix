#ifndef SamplingThread_h
#define SamplingThread_h

#include <qwt_sampling_thread.h>
#include <stdio.h>
#include "matrix/Keymaster.h"
#include "matrix/DataInterface.h"
#include "matrix/DataSink.h"
#include "matrix/TCondition.h"
#include "matrix/Thread.h"

class CurveData;

class SamplingThread: public QwtSamplingThread
{
    Q_OBJECT

public:
    SamplingThread(QObject *parent = NULL);

    bool set_stream_alias(std::string stream);
    bool set_keymaster_url(std::string key);
    bool set_display_field(std::string field);
    void pause(bool);
    void setData(CurveData *p);


protected:
    virtual void sample(double elapsed);

private:
    virtual double value(double timeStamp) ;
    void sink_reader_thread();

    std::unique_ptr<Keymaster> keymaster;
    std::unique_ptr<matrix::DataSink<matrix::GenericBuffer> > input_signal_sink;
    std::unique_ptr<matrix::data_description> ddesc;
    matrix::GenericBuffer gbuffer;
    Thread<SamplingThread> sink_thread;
    TCondition<bool> sink_thread_started;
    std::string ch1_fieldname;
    double d_last_value;
    bool paused;
    CurveData *data;
};
#endif
