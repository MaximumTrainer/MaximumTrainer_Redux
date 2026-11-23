#ifndef COURSESPAGE_H
#define COURSESPAGE_H

#include "racebot.h"
#include "racecourse.h"

#include <QIcon>
#include <QVector>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QLineEdit;
class QTreeWidget;

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

    /// The rider's profile (from Intervals.icu): sets the weight shown and the
    /// estimated times.
    /// \a fromIntervals: the weight comes from Intervals.icu and is shown
    /// read-only; otherwise it can be edited here (weightEdited).
    void setProfile(int ftpWatts, double weightKg, double bikeKg, double cda, bool fromIntervals);
    /// Your time riding \a course alone at \a watts.
    static double estimatedSeconds(const RaceCourse &course, double watts, double riderKg, double bikeKg, double cda);

signals:
    void rideRequested(const RaceCourse &course, const RaceBot::FieldSettings &bots);
    /// The rider changed their weight here (not signed in with Intervals.icu).
    void weightEdited(double weightKg);

private:
    QVector<RaceCourse> m_courses;
    QLineEdit *m_search = nullptr;
    QTreeWidget *m_courseList = nullptr;
    QLabel *m_weight = nullptr;
    QDoubleSpinBox *m_weightEdit = nullptr;
    int m_ftpWatts = 0;
    double m_bikeKg = 0.0;
    double m_cda = 0.0;
    QSpinBox *m_botCount = nullptr;
    QDoubleSpinBox *m_minWattsPerKg = nullptr;
    QDoubleSpinBox *m_maxWattsPerKg = nullptr;
};

#endif // COURSESPAGE_H
