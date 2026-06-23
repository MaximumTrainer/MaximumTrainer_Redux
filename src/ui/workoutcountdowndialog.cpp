#include "workoutcountdowndialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

WorkoutCountdownDialog::WorkoutCountdownDialog(const QString &nextWorkoutName,
                                               int countdownSeconds,
                                               QWidget *parent)
    : QDialog(parent)
    , m_remaining(countdownSeconds)
    , m_workoutName(nextWorkoutName)
{
    setWindowTitle(tr("Next Workout"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumWidth(380);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(16);
    layout->setContentsMargins(20, 20, 20, 20);

    m_label = new QLabel(this);
    m_label->setTextFormat(Qt::RichText);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    QFont f = m_label->font();
    f.setPointSize(f.pointSize() + 2);
    m_label->setFont(f);
    layout->addWidget(m_label);

    auto *btnRow = new QHBoxLayout();
    m_startBtn  = new QPushButton(tr("Start Now"), this);
    m_cancelBtn = new QPushButton(tr("Cancel Queue"), this);
    m_startBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_cancelBtn);
    layout->addLayout(btnRow);

    connect(m_startBtn,  &QPushButton::clicked, this, &WorkoutCountdownDialog::onStartNow);
    connect(m_cancelBtn, &QPushButton::clicked, this, &WorkoutCountdownDialog::onCancel);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &WorkoutCountdownDialog::tick);
    m_timer->start();

    updateLabel();
}

void WorkoutCountdownDialog::updateLabel()
{
    m_label->setText(tr("Next workout:<br><b>%1</b><br><br>Starting in <b>%2</b> second(s)…")
                     .arg(m_workoutName.toHtmlEscaped())
                     .arg(m_remaining));
}

void WorkoutCountdownDialog::tick()
{
    --m_remaining;
    if (m_remaining <= 0) {
        onStartNow();
        return;
    }
    updateLabel();
}

void WorkoutCountdownDialog::onStartNow()
{
    m_result = Result::StartNow;
    m_timer->stop();
    accept();
}

void WorkoutCountdownDialog::onCancel()
{
    m_result = Result::Cancelled;
    m_timer->stop();
    reject();
}
