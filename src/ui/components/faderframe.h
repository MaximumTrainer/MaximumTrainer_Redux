#ifndef FADERFRAME_H
#define FADERFRAME_H

#include <QFrame>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>



class FaderFrame : public QFrame
{
    Q_OBJECT

public:
    explicit FaderFrame(QWidget *parent = nullptr);

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

#endif // FADERFRAME_H
