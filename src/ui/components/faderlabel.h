#ifndef FADERLABEL_H
#define FADERLABEL_H

#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>


class FaderLabel : public QLabel
{
    Q_OBJECT

public:
    explicit FaderLabel(QWidget *parent = nullptr);

    void fadeIn(int durationMs);
    void fadeOut(int durationMs);

    void fadeOutAfterPause(int fadeDuration, int pause);
    void fadeInAndFadeOutAfterPause(int fadeInDuration, int fadeOutDuration, int pause);



signals:
    void clicked(bool stateClicked);
protected:
    void mousePressEvent ( QMouseEvent * event ) override ;


private slots:  //Used to connect with timer

    void fadeOutAfterTimer();


private :
    QTimer *timerAnimatorFadeOut;

    QGraphicsOpacityEffect *effect;
    QPropertyAnimation *anim;

    int tmpDurationFadeOut;


    bool stateClicked;

};

#endif // FADERLABEL_H



