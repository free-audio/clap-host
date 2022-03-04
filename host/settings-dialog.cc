#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "application.hh"
#include "settings-widget.hh"
#include "settings.hh"

#include "settings-dialog.hh"

SettingsDialog::SettingsDialog(Settings &settings, QWidget *parent)
   : QDialog(parent), _settings(settings) {

   Application::instance().engine()->stop();

   setModal(true);
   setWindowTitle(tr("Settings"));

   QVBoxLayout *vbox = new QVBoxLayout();
   _settingsWidget = new SettingsWidget(settings);
   vbox->addWidget(_settingsWidget);

   auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
   buttons->show();
   vbox->addWidget(buttons);
   connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

   setLayout(vbox);

   setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

SettingsDialog::~SettingsDialog() { Application::instance().engine()->start(); }