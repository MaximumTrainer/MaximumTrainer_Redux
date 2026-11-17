#ifndef Z_STYLESHEET_H
#define Z_STYLESHEET_H

#include <QMainWindow>

namespace Ui {
class Z_StyleSheet;
}

class Z_StyleSheet : public QMainWindow
{
    Q_OBJECT

public:
    explicit Z_StyleSheet(QWidget *parent = nullptr);
    ~Z_StyleSheet() override;

private:
    Ui::Z_StyleSheet *ui;
};

#endif // Z_STYLESHEET_H
