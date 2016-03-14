#include <qapplication.h>
#include "mainwindow.h"
#include "samplingthread.h"
#include <string>
#include <iostream>
using namespace std;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    MainWindow window;
    window.resize(800,400);

    window.initialize(argc, argv);

    window.show();

    window.start();

    bool ok = app.exec(); 

    window.stop();
    window.wait(1000);

    return ok;
}
