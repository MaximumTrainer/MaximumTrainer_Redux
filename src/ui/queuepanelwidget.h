#ifndef QUEUEPANELWIDGET_H
#define QUEUEPANELWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>

#include "workoutqueue.h"

/// Sidebar panel showing the current workout queue.
/// Supports removing entries and reordering via Move Up / Move Down.
class QueuePanelWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QueuePanelWidget(WorkoutQueue *queue, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onRemove();
    void onMoveUp();
    void onMoveDown();
    void onClear();

private:
    WorkoutQueue  *m_queue      = nullptr;
    QListWidget   *m_list       = nullptr;
    QLabel        *m_statusLbl  = nullptr;
    QPushButton   *m_removeBtn  = nullptr;
    QPushButton   *m_upBtn      = nullptr;
    QPushButton   *m_downBtn    = nullptr;
    QPushButton   *m_clearBtn   = nullptr;
};

#endif // QUEUEPANELWIDGET_H
