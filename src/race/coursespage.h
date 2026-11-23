#ifndef COURSESPAGE_H
#define COURSESPAGE_H

#include "racebot.h"
#include "racecourse.h"

#include <QIcon>
#include <QVector>
#include <QWidget>

class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QListWidget;

/// The Courses tab: pick a real road and ride it against a field of bots.
/// Your weight is shown for a check before starting, since it sets your
/// speed on the climbs.
class CoursesPage : public QWidget
{
    Q_OBJECT

public:
    explicit CoursesPage(QWidget *parent = nullptr);

    /// The courses bundled under :/courses, flattest first.
    static QVector<RaceCourse> bundledCourses();
    static double climbM(const RaceCourse &course);
    /// The left-bar icon: a climb profile in the gradient colours.
    static QIcon tabIcon();

    /// Refresh the weight from the profile (it can change in Settings).
    void setProfileWeightKg(double weightKg);

signals:
    void rideRequested(const RaceCourse &course, double riderWeightKg, const RaceBot::FieldSettings &bots);

private:
    QVector<RaceCourse> m_courses;
    QLineEdit *m_search = nullptr;
    QListWidget *m_courseList = nullptr;
    QDoubleSpinBox *m_weight = nullptr;
    QSpinBox *m_botCount = nullptr;
    QDoubleSpinBox *m_minWattsPerKg = nullptr;
    QDoubleSpinBox *m_maxWattsPerKg = nullptr;
};

#endif // COURSESPAGE_H
