#ifndef METRICCARD_H
#define METRICCARD_H

#include <QFrame>

class QVBoxLayout;

/// Rounded card around one workout metric (its info box and live graph).
/// Shows a coloured side bar for the target state, and hides itself when all
/// its contents are hidden (the per-metric display options toggle the parts).
class MetricCard : public QFrame
{
    Q_OBJECT
public:
    enum class State { None, OnTarget, Below, Above };

    explicit MetricCard(QWidget *parent = nullptr);

    /// A \a follower part (e.g. a trend chart) shows only while the card's
    /// other parts do; it never keeps the card visible on its own.
    void addPart(QWidget *part, int stretch = 0, bool follower = false);
    /// A small caption row for cards whose content has no title of its own.
    void setCaption(const QString &text);

public slots:
    /// -1 below, 0 on target, 1 above, 2 no target - as InfoWidget reports.
    void setTargetState(int state);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void syncVisibility();

    QVBoxLayout   *m_layout = nullptr;
    QList<QWidget *> m_parts;
    QList<QWidget *> m_followers;
};

#endif // METRICCARD_H
