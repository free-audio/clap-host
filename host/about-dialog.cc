#include <sstream>

#include <clap/version.h>

#include "about-dialog.hh"

AboutDialog::AboutDialog(QObject *parent) {
   std::ostringstream s;

   s << R"(# clap-host
Example host for [**CLAP**](https://github.com/free-audio/clap).

Sources on [Github](https://github.com/free-audio/clap-host).

clap-)" << CLAP_VERSION_MAJOR << "." << CLAP_VERSION_MINOR << "." << CLAP_VERSION_REVISION;

   setTextFormat(Qt::TextFormat::MarkdownText);
   setText(QString::fromStdString(s.str()));
}
