#ifndef TRENDCHART_H
#define TRENDCHART_H

#include <QColor>
#include <QElapsedTimer>
#include <QVector>
#include <QWidget>

/// Small live line chart of the last kWindowMs of one metric, for the metric
/// cards that have no target graph (speed, power balance, muscle oxygen).
/// Painted directly (no QWT): one polyline per repaint, at most a few per second.
class TrendChart : public QWidget
{
    Q_OBJECT
public:
    explicit TrendChart(const QColor &lineColor, QWidget *parent = nullptr);

    static constexpr qint64 kWindowMs = 30000;

    /// A dashed reference line (e.g. 50 for left/right balance) and an optional
    /// fixed value range; without one the range follows the data.
    void setReference(double value);
    void setFixedRange(double minimum, double maximum);

public slots:
    void addValue(double value);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Sample { qint64 ms; double value; };
    QVector<Sample> m_samples;
    QElapsedTimer   m_clock;
    QColor          m_lineColor;
    bool            m_hasReference = false;
    double          m_reference    = 0.0;
    bool            m_fixedRange   = false;
    double          m_min = 0.0, m_max = 1.0;
};

#endif // TRENDCHART_H
