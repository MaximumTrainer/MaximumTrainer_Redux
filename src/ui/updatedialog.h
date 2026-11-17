#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>

namespace Ui {
class UpdateDialog;
}

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(const QString &latestVersion, QWidget *parent = nullptr);
    ~UpdateDialog() override;
    void accept() override;


private:
    Ui::UpdateDialog *ui;


};

#endif // UPDATEDIALOG_H
