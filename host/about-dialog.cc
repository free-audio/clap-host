#include "about-dialog.hh"

AboutDialog::AboutDialog(QObject *parent) {
   setTextFormat(Qt::TextFormat::MarkdownText);
   setText(R"(# clap-host
Example host for [**CLAP**](https://github.com/free-audio/clap).

Sources on [Github](https://github.com/free-audio/clap-host).)");
}
