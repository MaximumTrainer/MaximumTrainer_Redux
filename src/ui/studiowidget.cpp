#include "studiowidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QApplication>
#include <QSignalBlocker>

#include "account.h"

StudioWidget::StudioWidget(QWidget *parent)
    : QWidget(parent)
{
    m_account = qApp->property("Account").value<Account*>();

    // Paint the page with the palette's Window colour (see SensorsWidget).
    setAutoFillBackground(true);
    buildUi();
    reload();
}

void StudioWidget::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel *title = new QLabel(tr("Studio"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    QGroupBox *group = new QGroupBox(tr("Studio Mode"), this);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setSpacing(4);

    m_enableCheck = new QCheckBox(tr("Enable Studio Mode"), group);
    connect(m_enableCheck, &QCheckBox::toggled,
            this, &StudioWidget::onStudioModeToggled);
    groupLayout->addWidget(m_enableCheck);

    QHBoxLayout *riderRow = new QHBoxLayout();
    riderRow->addWidget(new QLabel(tr("Number of riders:"), group));
    m_riderCountSpin = new QSpinBox(group);
    m_riderCountSpin->setRange(1, 6);
    connect(m_riderCountSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &StudioWidget::onRiderCountChanged);
    riderRow->addWidget(m_riderCountSpin);
    riderRow->addStretch();
    groupLayout->addLayout(riderRow);

    QLabel *note = new QLabel(
        tr("Studio mode lets multiple riders train simultaneously, each with "
           "their own sensors. Changes take effect after restarting a workout."),
        group);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    groupLayout->addWidget(note);

    mainLayout->addWidget(group);
    mainLayout->addStretch();
}

void StudioWidget::reload()
{
    if (!m_account)
        return;
    if (m_enableCheck) {
        QSignalBlocker b(m_enableCheck);
        m_enableCheck->setChecked(m_account->enable_studio_mode);
    }
    if (m_riderCountSpin) {
        QSignalBlocker b(m_riderCountSpin);
        m_riderCountSpin->setValue(qMax(1, m_account->nb_user_studio));
    }
}

void StudioWidget::onStudioModeToggled(bool enabled)
{
    emit studioModeChanged(enabled);
}

void StudioWidget::onRiderCountChanged(int nbRiders)
{
    emit riderCountChanged(nbRiders);
}
