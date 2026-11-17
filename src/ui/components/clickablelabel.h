#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>


class ClickableLabel : public QLabel
{
    Q_OBJECT

public:

    explicit ClickableLabel(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());


    void setVolumeLabel(bool isVolumeLabel) {
        this->isVolumeLabel = isVolumeLabel;
    }

signals:
    void clicked(bool stateClicked);

public slots:


protected:
    void mousePressEvent ( QMouseEvent * event ) override ;

private:
    bool stateClicked;
    bool isVolumeLabel;


};

#endif // CLICKABLELABEL_H
