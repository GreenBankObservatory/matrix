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

using namespace std;
using namespace matrix;

SamplingThread::SamplingThread(QObject *parent):
    QwtSamplingThread(parent),
    d_frequency(5.0),
    d_amplitude(20.0),
    keymaster(),
    input_signal_sink(),
    sink_thread(this, &SamplingThread::sink_reader_thread),
    sink_thread_started(false),
    d_last_value(0.0)
{

}

/// Takes a string specifying the url of the keymaster
bool SamplingThread::set_keymaster_url(std::string key)
{
    keymaster.reset(new Keymaster(key));
    input_signal_sink.reset(new DataSink<GenericBuffer>(key, 1000));
    return true;
}

/// Takes a string containing the alias name (e.g mydata)
/// Note this requires a couple of required sections in the keymaster config.
bool SamplingThread::set_stream_alias(std::string stream)
{
    string stream_alias;
    string component_name;
    string srcname;
    string stream_dd_path;
    YAML::Node dd_node;

    if (keymaster == nullptr)
    {
        cerr << "Need keymaster url" << endl;
        exit(-1);
    }

    try
    {
        stream_alias = string("streams." + stream);
        dd_node = keymaster->get(string(stream_alias));
    }
    catch(KeymasterException e)
    {
        cout << "Error getting stream alias key: " << stream_alias << endl;
        cout << e.what() << endl;
        return false;
    }

    if (dd_node.size() >= 3)
    {
        component_name = dd_node[0].as<string>();
        srcname = dd_node[1].as<string>();
        stream_dd_path = string("stream_descriptions.") + dd_node[2].as<string>();
    }
    else
    {
        cout << " Unexpected stream_description format| " << dd_node << endl;
        return false;
    }
    YAML::Node stream_dd;
    try
    {
        stream_dd = keymaster->get(stream_dd_path + ".fields");
    }
    catch(KeymasterException e)
    {
        cout << "Error getting key:" << stream_dd_path + ".fields" << endl;
        cout << e.what();
        return false;
    }
    ddesc.reset(new matrix::data_description(stream_dd));
    gbuffer.resize(ddesc->size());

    input_signal_sink->connect(component_name, srcname);

    sink_thread.start();
    sink_thread_started.wait(true);

    return true;
}

bool SamplingThread::set_display_field(std::string)
{
    return true;
}

void SamplingThread::setFrequency(double frequency)
{
    d_frequency = frequency;
    // printf("setting frequency to %lf\n", frequency);
    // keymaster.put("components.signal_generator.frequency", frequency, true);
}

double SamplingThread::frequency() const
{
    return d_frequency;
}

void SamplingThread::setAmplitude(double amplitude)
{
    d_amplitude = amplitude;
    // printf("setting amplitude to %lf\n", amplitude);
    // keymaster.put("components.signal_generator.amplitude", amplitude, true);
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

double SamplingThread::value(double)
{

    return d_last_value;
}

void SamplingThread::sink_reader_thread()
{
    sink_thread_started.signal(true);
    while (1)
    {
        double dd;
        bool found;
        try
        {
            input_signal_sink->flush(-1);
            input_signal_sink->get(gbuffer);
        } catch (MatrixException &e) {  }

        found = false;
        for (auto z = ddesc->fields.begin(); !found && z != ddesc->fields.end();)
        {
            // skip over unused fields
            if (z->skip)
            {
                ++z;
                continue;
            }
            // plot the first floating point non-skipped value
            switch (z->type)
            {
                case data_description::DOUBLE:
                {
                    found = true;
                    dd = matrix::get_data_buffer_value<double>(gbuffer.data(), z->offset);
                    break;
                }
                case data_description::FLOAT:
                {
                    found = true;
                    dd = (double) matrix::get_data_buffer_value<float>(gbuffer.data(), z->offset);
                    break;
                }
                default:
                    break;
            }
        }
        // printf("%f\n", dd);
        d_last_value = dd;
    }
}
