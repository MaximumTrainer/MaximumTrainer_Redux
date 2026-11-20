#ifndef SENSORSTATUSSTRIP_H
#define SENSORSTATUSSTRIP_H

#include <QMap>
#include <QWidget>

class QHBoxLayout;
class QTimer;
class QToolButton;

/// Row of chips in the workout window, one per sensor being connected:
/// a coloured dot and "Trainer · Connected". A sensor that was not found or
/// was lost can be clicked to try again. Pure UI: the owner feeds statuses.
/// Once every sensor is connected the strip steps aside after a few seconds,
/// and comes back as soon as one is not.
class SensorStatusStrip : public QWidget
{
    Q_OBJECT

public:
    enum class State { Searching, Connecting, Connected, NotFound, Lost };

    explicit SensorStatusStrip(QWidget *parent = nullptr);

    /// \a device (the saved device name) is shown after the role, elided if long.
    void addSensor(int key, const QString &name, const QString &device = QString());
    void setState(int key, State state);
    State state(int key) const { return m_states.value(key, State::Searching); }
    QString chipText(int key) const;

signals:
    void retryRequested(int key);

private:
    void refreshChip(int key);
    void updateVisibility();

    QTimer                     *m_hideTimer = nullptr;

    QHBoxLayout                *m_layout = nullptr;
    QMap<int, QToolButton*>     m_chips;
    QMap<int, QString>          m_names;
    QMap<int, QString>          m_devices;
    QMap<int, State>            m_states;
};

#endif // SENSORSTATUSSTRIP_H
