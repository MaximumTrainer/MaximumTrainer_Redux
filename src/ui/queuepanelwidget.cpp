#include "queuepanelwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

QueuePanelWidget::QueuePanelWidget(WorkoutQueue *queue, QWidget *parent)
    : QWidget(parent)
    , m_queue(queue)
{
    setWindowTitle(tr("Workout Queue"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // No title label here: the enclosing QDockWidget already shows "Workout Queue".

    m_list = new QListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::NoDragDrop);
    layout->addWidget(m_list, 1);

    // Start the queue from item #1.
    m_startBtn = new QPushButton(tr("▶ Start Queue"), this);
    m_startBtn->setToolTip(tr("Start the first workout in the queue"));
    layout->addWidget(m_startBtn);

    // Button row
    auto *btnRow = new QHBoxLayout();
    m_upBtn   = new QPushButton(tr("▲"), this);
    m_downBtn = new QPushButton(tr("▼"), this);
    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_clearBtn  = new QPushButton(tr("Clear"), this);
    m_upBtn->setMaximumWidth(32);
    m_downBtn->setMaximumWidth(32);
    m_upBtn->setToolTip(tr("Move selected workout up"));
    m_downBtn->setToolTip(tr("Move selected workout down"));
    m_removeBtn->setToolTip(tr("Remove selected workout from queue"));
    m_clearBtn->setToolTip(tr("Clear entire queue"));
    btnRow->addWidget(m_upBtn);
    btnRow->addWidget(m_downBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_clearBtn);
    layout->addLayout(btnRow);

    m_statusLbl = new QLabel(this);
    m_statusLbl->setStyleSheet("color: #888; font-style: italic;");
    layout->addWidget(m_statusLbl);

    connect(m_startBtn,  &QPushButton::clicked, this, &QueuePanelWidget::startQueueRequested);
    connect(m_removeBtn, &QPushButton::clicked, this, &QueuePanelWidget::onRemove);
    connect(m_upBtn,     &QPushButton::clicked, this, &QueuePanelWidget::onMoveUp);
    connect(m_downBtn,   &QPushButton::clicked, this, &QueuePanelWidget::onMoveDown);
    connect(m_clearBtn,  &QPushButton::clicked, this, &QueuePanelWidget::onClear);

    connect(m_queue, &WorkoutQueue::queueChanged, this, &QueuePanelWidget::refresh);
    refresh();
}

void QueuePanelWidget::refresh()
{
    m_list->clear();
    for (int i = 0; i < m_queue->count(); ++i)
        m_list->addItem(QString("%1. %2").arg(i + 1).arg(m_queue->name(i)));

    const bool hasItems = !m_queue->isEmpty();
    m_startBtn->setEnabled(hasItems);
    m_removeBtn->setEnabled(hasItems);
    m_upBtn->setEnabled(hasItems);
    m_downBtn->setEnabled(hasItems);
    m_clearBtn->setEnabled(hasItems);

    if (m_queue->isEmpty()) {
        m_statusLbl->setText(tr("Queue is empty."));
    } else {
        m_statusLbl->setText(tr("%1 workout(s) queued").arg(m_queue->count()));
    }
}

void QueuePanelWidget::onRemove()
{
    int row = m_list->currentRow();
    if (row >= 0) m_queue->removeAt(row);
}

void QueuePanelWidget::onMoveUp()
{
    int row = m_list->currentRow();
    if (row > 0) {
        m_queue->moveUp(row);
        m_list->setCurrentRow(row - 1);
    }
}

void QueuePanelWidget::onMoveDown()
{
    int row = m_list->currentRow();
    if (row >= 0 && row < m_queue->count() - 1) {
        m_queue->moveDown(row);
        m_list->setCurrentRow(row + 1);
    }
}

void QueuePanelWidget::onClear()
{
    m_queue->clear();
}
