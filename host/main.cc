#include <cstdlib>

#include <QApplication>

#include "application.hh"

bool zeroOutParamCookies{false};

int main(int argc, char *argv[]) {

#ifdef Q_OS_LINUX
   ::setenv("QT_QPA_PLATFORM", "xcb", 1);
#endif

   if (getenv("ZERO_COOKIES"))
      zeroOutParamCookies = true;

   QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
   Application app(argc, argv);
   return app.exec();
}
