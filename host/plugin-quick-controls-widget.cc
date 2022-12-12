#include <QComboBox>
#include <QGridLayout>
#include <QVBoxLayout>

#include "plugin-host.hh"
#include "plugin-quick-control-widget.hh"
#include "plugin-quick-controls-widget.hh"

PluginQuickControlsWidget::PluginQuickControlsWidget(QWidget *parent, PluginHost &pluginHost)
   : QWidget(parent), _pluginHost(pluginHost) {
   _chooser = new QComboBox(this);

   for (auto &qc : _controls)
      qc = new PluginQuickControlWidget(this, pluginHost);

   auto grid = new QGridLayout();
   grid->setSpacing(3);

   const auto rowSize = CLAP_REMOTE_CONTROLS_COUNT / 2;
   for (int i = 0; i < CLAP_REMOTE_CONTROLS_COUNT; ++i)
      grid->addWidget(_controls[i], i / rowSize, i % rowSize);

   auto vbox = new QVBoxLayout();
   vbox->addWidget(_chooser);
   vbox->addLayout(grid);
   vbox->setSpacing(3);
   setLayout(vbox);

   connect(
      _chooser, &QComboBox::activated, this, &PluginQuickControlsWidget::selectPageFromChooser);

   connect(&pluginHost,
           &PluginHost::quickControlsPagesChanged,
           this,
           &PluginQuickControlsWidget::pagesChanged);

   connect(&pluginHost,
           &PluginHost::quickControlsSelectedPageChanged,
           this,
           &PluginQuickControlsWidget::selectedPageChanged);
}

void PluginQuickControlsWidget::pagesChanged() {
   auto &pages = _pluginHost.remoteControlsPages();
   _chooser->clear();
   for (auto &it : pages)
      _chooser->addItem(it->page_name, it->page_id);

   selectedPageChanged();
}

void PluginQuickControlsWidget::selectedPageChanged() {
   auto pageId = _pluginHost.remoteControlsSelectedPage();
   auto &pagesIndex = _pluginHost.remoteControlsPagesIndex();
   auto &params = _pluginHost.params();
   auto it = pagesIndex.find(pageId);

   for (int i = 0; i < CLAP_REMOTE_CONTROLS_COUNT; ++i) {
      PluginParam *param = nullptr;

      if (it != pagesIndex.end()) {
         auto paramId = it->second->param_ids[i];
         auto paramIt = params.find(paramId);
         if (paramIt != params.end())
            param = paramIt->second.get();
      }

      _controls[i]->setPluginParam(param);
   }
}

void PluginQuickControlsWidget::selectPageFromChooser(int index) {
   clap_id pageId = _chooser->itemData(index).toUInt();
   _pluginHost.setRemoteControlsSelectedPageByHost(pageId);
   selectedPageChanged();
}
