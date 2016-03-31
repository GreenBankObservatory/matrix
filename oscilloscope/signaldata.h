#ifndef _SIGNAL_DATA_H_
#define _SIGNAL_DATA_H_ 1

#include <qrect.h>

class SignalData
{
public:
    // static SignalData &instance();
    SignalData();
    virtual ~SignalData();
    void append(const QPointF &pos);
    void clearStaleValues(double min);

    int size() const;
    QPointF value(int index) const;

    QRectF boundingRect() const;

    void lock();
    void unlock();
    
private:
    SignalData(const SignalData &);
    SignalData &operator=( const SignalData & );



    class PrivateData;
    PrivateData *d_data;
};

#endif
