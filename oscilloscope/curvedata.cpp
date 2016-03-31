#include "curvedata.h"
#include "signaldata.h"

//const SignalData &CurveData::values() const
//{
//    return SignalData::instance();
//}

//SignalData &CurveData::values()
//{
//    return SignalData::instance();
//}
CurveData::CurveData()
{

}

QPointF CurveData::sample(size_t i) const
{
    return myvalues.value(i);
}

size_t CurveData::size() const
{
    return myvalues.size();
}

QRectF CurveData::boundingRect() const
{
    return myvalues.boundingRect();
}
