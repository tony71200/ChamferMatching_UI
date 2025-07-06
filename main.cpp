#include "matchingui.h"
#include <pylon/PylonIncludes.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    Pylon::PylonAutoInitTerm auto_init_term;
    QApplication a(argc, argv);
    MatchingUI w;
    w.show();

    return a.exec();
}
