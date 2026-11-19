#include "trendchart.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>

TrendChart::TrendChart(const QColor &lineColor, QWidget *parent)
    : QWidget(parent), m_lineColor(lineColor)
{
    setMinimumHeight(80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_clock.start();
}

void TrendChart::setReference(double value)
{
    m_hasReference = true;
    m_reference = value;
}

void TrendChart::setFixedRange(double minimum, double maximum)
{
    m_fixedRange = true;
    m_min = minimum;
    m_max = maximum;
}

void TrendChart::addValue(double value)
{
    if (value < 0)
        return;
    const qint64 now = m_clock.elapsed();
    m_samples.append({now, value});
    while (!m_samples.isEmpty() && m_samples.first().ms < now - kWindowMs)
        m_samples.removeFirst();
    update();
}

void TrendChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(31, 31, 37));
    painter.drawRoundedRect(area, 6, 6);

    const QRectF plot = area.adjusted(6, 8, -6, -16);
    painter.setPen(QColor(109, 109, 120));
    QFont small = font();
    small.setPointSizeF(qMax(6.0, small.pointSizeF() - 2));
    painter.setFont(small);
    painter.drawText(area.adjusted(6, 0, -6, -2), Qt::AlignLeft | Qt::AlignBottom, tr("-30s"));
    painter.drawText(area.adjusted(6, 0, -6, -2), Qt::AlignRight | Qt::AlignBottom, tr("now"));
    if (m_samples.size() < 2)
        return;

    double low = m_min, high = m_max;
    if (!m_fixedRange) {
        const auto [lo, hi] = std::minmax_element(m_samples.cbegin(), m_samples.cend(),
            [](const Sample &a, const Sample &b) { return a.value < b.value; });
        const double pad = qMax(1.0, (hi->value - lo->value) * 0.2);
        low = lo->value - pad;
        high = hi->value + pad;
    }
    const qint64 now = m_clock.elapsed();
    auto toPoint = [&](const Sample &s) {
        const double x = plot.right() - plot.width() * double(now - s.ms) / kWindowMs;
        const double y = plot.bottom() - plot.height() * (qBound(low, s.value, high) - low) / (high - low);
        return QPointF(x, y);
    };

    if (m_hasReference) {
        const double y = plot.bottom() - plot.height() * (m_reference - low) / (high - low);
        painter.setPen(QPen(QColor(85, 85, 95), 1, Qt::DashLine));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    QPainterPath path(toPoint(m_samples.first()));
    for (int i = 1; i < m_samples.size(); ++i)
        path.lineTo(toPoint(m_samples.at(i)));
    painter.setPen(QPen(m_lineColor, 1.8));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.setBrush(m_lineColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(toPoint(m_samples.last()), 2.5, 2.5);
}
