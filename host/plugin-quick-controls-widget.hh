#pragma once

#include <QWidget>

#include <clap/clap.h>

QT_BEGIN_NAMESPACE
class QComboBox;
QT_END_NAMESPACE

class PluginHost;
class PluginQuickControlWidget;
class PluginQuickControlsWidget : public QWidget {
   Q_OBJECT;

public:
   PluginQuickControlsWidget(QWidget *parent, PluginHost &pluginHost);

   void pagesChanged();
   void selectedPageChanged();

private:
   void selectPageFromChooser(int index);

   PluginHost &_pluginHost;

   QComboBox *_chooser = nullptr;
   std::array<PluginQuickControlWidget *, CLAP_REMOTE_CONTROLS_COUNT> _controls;
};