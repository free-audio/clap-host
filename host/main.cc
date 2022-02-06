#include <cstdio>

#include <QApplication>

#include "application.hh"

int main(int argc, char *argv[]) {
   //QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
   Application app(argc, argv);
   return app.exec();
}
