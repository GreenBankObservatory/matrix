#ifndef _WHEELBOX_H_
#define _WHEELBOX_H_

#include <qwidget.h>

class QwtWheel;
class QLabel;
class QLCDNumber;

class WheelBox: public QWidget
{
    Q_OBJECT

public:
    WheelBox(const QString &title, 
        double min, double max, double stepSize, 
        QWidget *parent = NULL);

    void setUnit(const QString &);
    QString unit() const;

    void setValue(double value);
    double value() const;

Q_SIGNALS:
    double valueChanged(double);

public Q_SLOTS:
    void adjustPrimary(double);
    void adjustFine(double);
    void adjustValue(double);

private:
    QLCDNumber *d_number;
    QwtWheel *d_wheel;
    QwtWheel *d_fine;
    QLabel *d_label;
    double coarse;
    double fine;

    QString d_unit;
};

#endif
