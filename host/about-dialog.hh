#include <QMessageBox>

class AboutDialog : public QMessageBox {
   Q_OBJECT
   using super = QMessageBox;

public:
   AboutDialog(QObject *parent);
};