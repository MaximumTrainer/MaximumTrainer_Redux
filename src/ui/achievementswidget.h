#ifndef ACHIEVEMENTSWIDGET_H
#define ACHIEVEMENTSWIDGET_H

#include <QPixmap>
#include <QWidget>

class ManagerAchievement;
class QLabel;
class QProgressBar;
class QVBoxLayout;

/// Main-window page listing every achievement by category: earned ones with
/// their date, locked ones greyed out with how to earn them and the progress.
class AchievementsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AchievementsWidget(QWidget *parent = nullptr);

    /// Re-reads the ride history (awarding anything it reached) and redraws.
    void reload();

private:
    void render();
    QWidget *buildCard(const QString &key, QWidget *parent);

    ManagerAchievement *m_manager     = nullptr;
    QLabel             *m_summary     = nullptr;
    QProgressBar       *m_overall     = nullptr;
    QWidget            *m_content     = nullptr;
    QVBoxLayout        *m_sections    = nullptr;
    QPixmap             m_trophy;
    QPixmap             m_trophyLocked;
};

#endif // ACHIEVEMENTSWIDGET_H
