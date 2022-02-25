#include <cstdlib>

#include <QApplication>

#include "application.hh"

int main(int argc, char *argv[]) {

#ifdef Q_OS_LINUX
   ::setenv("QT_QPA_PLATFORM", "xcb", 1);
#endif

   QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
   Application app(argc, argv);
   return app.exec();
}
