#include "dialog_connection_method.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

DialogConnectionMethod::DialogConnectionMethod(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Select Connection Method"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    auto *label = new QLabel(
        tr("How do you want to connect your sensors for this workout?"), this);
    label->setWordWrap(true);
    layout->addWidget(label);

    auto *btnLayout = new QHBoxLayout;
    layout->addLayout(btnLayout);

    auto *btnBtle = new QPushButton(tr("BTLE Device"), this);
    auto *btnSim  = new QPushButton(tr("Simulation"),  this);

    btnBtle->setObjectName(QStringLiteral("btn_btle"));
    btnSim->setObjectName(QStringLiteral("btn_sim"));
    btnBtle->setDefault(true);
    btnLayout->addWidget(btnBtle);
    btnLayout->addWidget(btnSim);

    connect(btnBtle, &QPushButton::clicked, this, [this]() {
        m_method = BTLE;
        accept();
    });
    connect(btnSim, &QPushButton::clicked, this, [this]() {
        m_method = Simulation;
        accept();
    });
}
