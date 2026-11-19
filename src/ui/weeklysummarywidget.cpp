#include "weeklysummarywidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QToolTip>
#include <QVBoxLayout>
#include <QHelpEvent>

#include "workouthistorysummary.h"

namespace {
QString hoursText(double hours)
{
    const int minutes = qRound(hours * 60);
    return QStringLiteral("%1:%2").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

// "▲ 40" / "▼ 12" / "=" relative to last week.
QString deltaText(double now, double before, int decimals = 0, const QString &unit = QString())
{
    if (now == 0.0 && before == 0.0)
        return QString();
    const double delta = now - before;
    if (qAbs(delta) < (decimals ? 0.05 : 0.5))
        return QStringLiteral("<span style='color:#9a9aa6'>= last week</span>");
    const QString number = QLocale().toString(qAbs(delta), 'f', decimals) + unit;
    return delta > 0 ? QStringLiteral("<span style='color:#3fb27f'>▲ %1</span>").arg(number)
                     : QStringLiteral("<span style='color:#e5534b'>▼ %1</span>").arg(number);
}

QLabel *makeTile(QWidget *parent, QHBoxLayout *row)
{
    auto *tile = new QLabel(parent);
    tile->setTextFormat(Qt::RichText);
    tile->setObjectName(QStringLiteral("weekTile"));
    tile->setMinimumWidth(110);
    row->addWidget(tile);
    return tile;
}
} // namespace

// 12 bars of weekly TSS, the current week highlighted.
class WeeklySummaryWidget::TssBars : public QWidget
{
public:
    explicit TssBars(QWidget *parent) : QWidget(parent)
    {
        setMinimumSize(260, 70);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    void setWeeks(const QVector<WeeklySummary::Week> &weeks) { m_weeks = weeks; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF area = QRectF(rect()).adjusted(2, 4, -2, -16);
        double top = 1.0;
        for (const auto &week : m_weeks)
            top = qMax(top, week.tss);
        const int count = m_weeks.size();
        if (count == 0)
            return;
        const double slot = area.width() / count;
        QFont small = font();
        small.setPointSizeF(qMax(6.0, small.pointSizeF() - 2));
        painter.setFont(small);
        for (int i = 0; i < count; ++i) {
            const double height = area.height() * m_weeks[i].tss / top;
            const QRectF bar(area.left() + i * slot + slot * 0.15, area.bottom() - height,
                             slot * 0.7, qMax(height, 1.5));
            const bool current = i == count - 1;
            painter.setPen(Qt::NoPen);
            painter.setBrush(current ? QColor(242, 184, 48) : QColor(110, 140, 200));
            painter.drawRoundedRect(bar, 2, 2);
            if (i % 3 == 2 || current) {
                painter.setPen(QColor(140, 140, 150));
                painter.drawText(QRectF(area.left() + i * slot - 10, area.bottom() + 2, slot + 20, 14),
                                 Qt::AlignHCenter | Qt::AlignTop,
                                 current ? tr("This wk") : QLocale().toString(m_weeks[i].monday, QStringLiteral("d MMM")));
            }
        }
    }
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::ToolTip && !m_weeks.isEmpty()) {
            auto *help = static_cast<QHelpEvent *>(e);
            const int index = qBound(0, int(help->pos().x() / (width() / double(m_weeks.size()))),
                                     int(m_weeks.size()) - 1);
            const auto &week = m_weeks[index];
            QToolTip::showText(help->globalPos(),
                tr("Week of %1\n%2 rides, %3 h, %4 TSS, %5 km")
                    .arg(QLocale().toString(week.monday, QStringLiteral("d MMM yyyy")))
                    .arg(week.rides).arg(hoursText(week.hours)).arg(qRound(week.tss))
                    .arg(QLocale().toString(week.km, 'f', 1)), this);
            return true;
        }
        return QWidget::event(e);
    }

private:
    QVector<WeeklySummary::Week> m_weeks;
};

WeeklySummaryWidget::WeeklySummaryWidget(QWidget *parent) : QWidget(parent)
{
    setStyleSheet(QStringLiteral(
        "QLabel#weekTile { background: palette(base); border: 1px solid palette(mid);"
        " border-radius: 8px; padding: 6px 10px; }"));
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    m_rides = makeTile(this, row);
    m_time  = makeTile(this, row);
    m_tss   = makeTile(this, row);
    m_km    = makeTile(this, row);
    m_bars  = new TssBars(this);
    row->addWidget(m_bars, 1);
    setHistory({});
}

void WeeklySummaryWidget::setHistory(const QList<WorkoutHistorySummary> &history)
{
    const QVector<WeeklySummary::Week> weeks =
        WeeklySummary::recentWeeks(history, QDate::currentDate(), kWeeks);
    const WeeklySummary::Week &now  = weeks.last();
    const WeeklySummary::Week &prev = weeks.at(weeks.size() - 2);
    const auto tile = [](const QString &caption, const QString &value, const QString &delta) {
        return QStringLiteral("<span style='color:#9a9aa6;font-size:8pt'>%1</span><br>"
                              "<span style='font-size:16pt;font-weight:700'>%2</span> %3")
            .arg(caption, value, delta);
    };
    m_rides->setText(tile(tr("THIS WEEK · RIDES"), QString::number(now.rides), deltaText(now.rides, prev.rides)));
    m_time->setText(tile(tr("TIME"), hoursText(now.hours), deltaText(now.hours, prev.hours, 1, QStringLiteral(" h"))));
    m_tss->setText(tile(tr("TSS"), QString::number(qRound(now.tss)), deltaText(now.tss, prev.tss)));
    m_km->setText(tile(tr("DISTANCE"), QLocale().toString(now.km, 'f', 1) + QStringLiteral(" km"),
                       deltaText(now.km, prev.km, 1, QStringLiteral(" km"))));
    m_bars->setWeeks(weeks);
    m_bars->setToolTip(QString());
}
