#ifndef ACHIEVEMENTTOAST_H
#define ACHIEVEMENTTOAST_H

#include <QQueue>
#include <QWidget>

#include "achievement.h"

class QGraphicsOpacityEffect;
class QLabel;
class QSequentialAnimationGroup;

/// In-workout "Achievement unlocked" banner: slides down from the top of its
/// parent with a small bounce, holds, then slides back up. Several unlocks queue
/// and play one after another. Never takes mouse input.
class AchievementToast : public QWidget
{
    Q_OBJECT
public:
    explicit AchievementToast(QWidget *parent);

    void enqueue(const Achievement &achievement);

    static constexpr int kHoldMs = 4000;

signals:
    /// A banner started showing (the dialog plays the achievement sound).
    void shown(const Achievement &achievement);

private:
    void playNext();
    QPoint restingPosition() const;

    QQueue<Achievement>        m_queue;
    QLabel                    *m_name        = nullptr;
    QLabel                    *m_description = nullptr;
    QGraphicsOpacityEffect    *m_opacity     = nullptr;
    QSequentialAnimationGroup *m_animation   = nullptr;
};

#endif // ACHIEVEMENTTOAST_H
