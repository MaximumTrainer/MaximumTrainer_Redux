#ifndef REPEATWIDGET_H
#define REPEATWIDGET_H

#include <QWidget>
#include <QLabel>
#include <repeatdata.h>

namespace Ui {
class RepeatWidget;
}

class RepeatWidget : public QWidget
{
    Q_OBJECT

public:
    RepeatWidget(RepeatData *data, QWidget *parent = 0);
    ~RepeatWidget();


    void setRightWidth(int width);
    bool eventFilter(QObject *watched, QEvent *event);


    static bool myLessThan(RepeatWidget* p1, RepeatWidget* p2);

    RepeatData* getRepeatData() {
        return this->data;
    }



signals:
    void clickedRightPartWidget();

    void deleteSignal(int id);
    void updateSignal(int id);


private slots:
    void on_pushButton_delete_clicked();
    // Connected explicitly (see constructor): Qt6 removed
    // QComboBox::currentIndexChanged(const QString&), so the old
    // connectSlotsByName auto-connection silently no longer fired.
    void onRepeatCountChanged(const QString &arg1);


private:
    Ui::RepeatWidget *ui;
    RepeatData *data;

//     void moveEvent (QMoveEvent * event);
};

#endif // REPEATWIDGET_H
