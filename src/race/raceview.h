#ifndef RACEVIEW_H
#define RACEVIEW_H

#include <QPointF>
#include <QVector>
#include <QWidget>

class RaceSimulation;

/// The race screen: progress bar, the route seen from above with every rider
/// as a numbered dot, the standings, and the whole course coloured by
/// gradient with what is coming next. Drawn natively; it only reads the
/// simulation, so call update() after each race step.
class RaceView : public QWidget
{
    Q_OBJECT

public:
    /// \a yourRiderKg is your body weight alone, for the W/kg readout.
    RaceView(const RaceSimulation *race, double yourRiderKg, QWidget *parent = nullptr);

    QSize sizeHint() const override { return {960, 520}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void paintTopBar(QPainter &painter, const QRectF &area) const;
    void paintRoute(QPainter &painter, const QRectF &area) const;
    void paintNowPanel(QPainter &painter, const QRectF &mapArea) const;
    void paintStandings(QPainter &painter, const QRectF &area) const;
    void paintProfile(QPainter &painter, const QRectF &area) const;
    void paintFinish(QPainter &painter, const QRectF &area) const;

    QPointF routePoint(double raceDistanceM, const QRectF &area) const;
    QColor gradeColor(double grade) const;

    const RaceSimulation *m_race;
    double m_yourRiderKg;
    // The lap's route projected flat once (x east, y north, in metres).
    QVector<QPointF> m_projected;
    QRectF m_projectedBounds;
};

#endif // RACEVIEW_H
