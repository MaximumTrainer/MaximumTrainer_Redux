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
    explicit RaceView(const RaceSimulation *race, QWidget *parent = nullptr);

    QSize sizeHint() const override { return {960, 520}; }

    /// Your W/kg turns red above this (your FTP over your weight), and a
    /// rider's dot flashes red at 1.5 times it (a sprint). 0 = never.
    void setRedLineWattsPerKg(double wattsPerKg);

    /// The virtual gear, drawn top left of the map with ▼/▲ buttons.
    void setGear(int gear, int gearCount, bool visible);
    /// Light the ▲ (direction > 0) or ▼ button briefly after a shift.
    void flashShift(int direction);
    /// Cadence and heart rate for the card (power comes from the race). 0 = none.
    void setLiveMetrics(int cadenceRpm, int heartRateBpm);

    /// The lane (0 to 3, across the road) a rider's dot is in, as last drawn.
    int laneOf(int riderIndex) const { return riderIndex < m_lanes.size() ? m_lanes.at(riderIndex) : -1; }

signals:
    void gearUpClicked();
    void gearDownClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void paintTopBar(QPainter &painter, const QRectF &area) const;
    void paintRoute(QPainter &painter, const QRectF &area) const;
    void paintOverview(QPainter &painter, const QRectF &mapArea) const;
    QRectF gearPanelRect(const QRectF &mapArea) const;
    void paintGearPanel(QPainter &painter, const QRectF &mapArea) const;
    QRectF nowCardRect(const QRectF &mapArea) const;
    void paintNowPanel(QPainter &painter, const QRectF &mapArea) const;
    void paintStandings(QPainter &painter, const QRectF &area) const;
    void paintProfile(QPainter &painter, const QRectF &area) const;
    void paintFinish(QPainter &painter, const QRectF &area) const;

    QPointF project(double raceDistanceM) const;   // metres east/north of the start
    QRectF zoomBounds() const;                     // the stretch of road around you
    double gapM(int riderIndex) const;
    bool aboveRedLine(double wattsPerKg) const;
    bool aboveSprintLine(double wattsPerKg) const;             // metres ahead of you (negative: behind)
    QPointF routePoint(double raceDistanceM, const QRectF &area, const QRectF &bounds, double margin = 28.0) const;
    QColor gradeColor(double grade) const;

    const RaceSimulation *m_race;
    // The lap's route projected flat once (x east, y north, in metres).
    QVector<QPointF> m_projected;
    QRectF m_projectedBounds;
    double m_redLineWattsPerKg = 0.0;
    // Each rider's lane across the road (0 to 3) and where their dot is on
    // its way to it; kept between frames so riders don't hop sides.
    mutable QVector<int> m_lanes;
    mutable QVector<int> m_tagSides;          // which side of its dot each name tag sits
    int m_cadenceRpm = 0;
    int m_heartRateBpm = 0;
    int m_gear = 1;
    int m_gearCount = 1;
    bool m_gearVisible = false;
    int m_shiftDirection = 0;
    qint64 m_shiftAtMs = 0;
    mutable QRectF m_gearUpRect;
    mutable QRectF m_gearDownRect;
    mutable QVector<double> m_laneOffsets;
};

#endif // RACEVIEW_H
