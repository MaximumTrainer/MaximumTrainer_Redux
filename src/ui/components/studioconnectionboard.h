#ifndef STUDIOCONNECTIONBOARD_H
#define STUDIOCONNECTIONBOARD_H

#include <QMap>
#include <QPair>
#include <QWidget>

#include "sensorstatusstrip.h"

class QGridLayout;
class QLabel;
class QFrame;

/// Studio before the start: one card per rider listing their sensors as they
/// connect ("19 of 22 connected"). A sensor that was not found or was lost can
/// be clicked to try again. Pure UI: the owner feeds the states.
class StudioConnectionBoard : public QWidget
{
    Q_OBJECT

public:
    explicit StudioConnectionBoard(QWidget *parent = nullptr);

    void addRider(int rider, const QString &name);
    void addSensor(int rider, int key, const QString &role, const QString &device);
    void setState(int rider, int key, SensorStatusStrip::State state);

    int connectedCount() const;
    int sensorCount() const { return int(m_states.size()); }
    bool allConnected() const { return sensorCount() > 0 && connectedCount() == sensorCount(); }
    QString statusText(int rider, int key) const;

signals:
    void retryRequested(int rider, int key);
    void allConnectedChanged(bool allConnected);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Row { QLabel *dot = nullptr; QLabel *status = nullptr; };
    using Key = QPair<int, int>;

    void refreshHeader();
    void refreshCard(int rider);

    QLabel      *m_count = nullptr;
    QGridLayout *m_grid = nullptr;
    QMap<int, QFrame*>                         m_cards;
    QMap<int, QLabel*>                         m_ready;
    QMap<Key, Row>                             m_rows;
    QMap<Key, SensorStatusStrip::State>        m_states;
    bool m_wasAllConnected = false;
};

#endif // STUDIOCONNECTIONBOARD_H
