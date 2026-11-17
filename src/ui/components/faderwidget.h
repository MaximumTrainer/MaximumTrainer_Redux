#ifndef FADERWIDGET_H
#define FADERWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>


class FaderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FaderWidget(QWidget *parent = nullptr);

    void fadeIn(int durationMs);
    void fadeOut(int durationMs);

    void fadeOutAfterPause(int fadeDuration, int pause);
    void fadeInAndFadeOutAfterPause(int fadeInDuration, int fadeOutDuration, int pause);


private slots:  //Used to connect with timer

    void fadeOutAfterTimer();


private :
    QTimer *timerAnimatorFadeOut;

    QGraphicsOpacityEffect *effect;
    QPropertyAnimation *anim;

    int tmpDurationFadeOut;

};

#endif // FADERWIDGET_H
