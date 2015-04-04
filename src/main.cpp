#include <QApplication>
#include "pcmdft.h"


int main (int argc, char** argv)
{
    QApplication app (argc, argv);
    PCMDFT::pcmdft foo;
    foo.show();
    return app.exec();
}
