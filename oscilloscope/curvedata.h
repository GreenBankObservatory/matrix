#include <qwt_series_data.h>
#include <qpointer.h>

#include "signaldata.h"

class CurveData: public QwtSeriesData<QPointF>
{
public:
    CurveData();

    const SignalData &values() const
    { return myvalues; }

    SignalData &values()
    { return myvalues; }

    virtual QPointF sample(size_t i) const;
    virtual size_t size() const;

    virtual QRectF boundingRect() const;
    SignalData myvalues;
};
