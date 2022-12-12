#include <QCheckBox>
#include <QVBoxLayout>
#include <QDialogButtonBox>

#include "plugin-host-settings.hh"
#include "settings.hh"
#include "tweaks-dialog.hh"

TweaksDialog::TweaksDialog(Settings &settings, QWidget *parent)
   : QDialog(parent) {
   setModal(true);
   setWindowTitle(tr("Tweaks"));
   setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

   auto vbox = new QVBoxLayout;
   setLayout(vbox);

   auto &pluginHostSettings = settings.pluginHostSettings();
   auto cookieCheckBox = new QCheckBox(tr("Provide Cookie"), this);
   cookieCheckBox->setChecked(pluginHostSettings.shouldProvideCookie());
   cookieCheckBox->setToolTip(
      tr("If enabled passes the cookie value to the plugin in parameter related events."));
   connect(cookieCheckBox, &QCheckBox::stateChanged, [&pluginHostSettings](int state) {
      pluginHostSettings.setShouldProvideCookie(state);
   });
   vbox->addWidget(cookieCheckBox);

   auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
   buttons->show();
   vbox->addWidget(buttons);
   connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
}
