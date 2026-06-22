#include "dialogkeyboardshortcuts.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QTabWidget>

static void addRow(QTableWidget *table, int row, const QString &key, const QString &action)
{
    table->setItem(row, 0, new QTableWidgetItem(key));
    table->setItem(row, 1, new QTableWidgetItem(action));
}

static QTableWidget *makeTable(const QVector<QPair<QString,QString>> &rows)
{
    auto *table = new QTableWidget(rows.size(), 2);
    table->setHorizontalHeaderLabels({QObject::tr("Key"), QObject::tr("Action")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setAlternatingRowColors(true);
    for (int i = 0; i < rows.size(); ++i)
        addRow(table, i, rows[i].first, rows[i].second);
    return table;
}

DialogKeyboardShortcuts::DialogKeyboardShortcuts(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    setMinimumSize(480, 400);

    auto *tabs = new QTabWidget(this);

    // ----- Workout Player tab -----
    const QVector<QPair<QString,QString>> workoutRows = {
        {tr("Space  /  Enter"),  tr("Start / Pause workout")},
        {tr("→  (Right)"),       tr("Skip to next interval")},
        {tr("↑  /  ↓"),          tr("Virtual shift up / down (or difficulty if shifting is off)")},
        {tr("+  /  ="),          tr("Increase difficulty +5 %")},
        {tr("-"),                tr("Decrease difficulty -5 %")},
        {tr("L  /  Backspace"),  tr("Manual lap")},
        {tr("F6 / F7 / F8"),     tr("Radio: previous / play-pause / next")},
        {tr("F11"),              tr("Toggle fullscreen")},
        {tr("?  /  F1"),         tr("Show keyboard shortcuts")},
        {tr("Escape"),           tr("Exit workout (with confirmation)")},
    };
    tabs->addTab(makeTable(workoutRows), tr("Workout Player"));

    // ----- Main Window tab -----
    const QVector<QPair<QString,QString>> mainRows = {
        {tr("Ctrl+,"), tr("Open Preferences")},
        {tr("Ctrl+N"), tr("Create new workout")},
        {tr("Ctrl+Q"), tr("Quit application")},
        {tr("F1"),     tr("Show keyboard shortcuts")},
    };
    tabs->addTab(makeTable(mainRows), tr("Main Window"));

    // ----- Zwift Click (right controller) tab -----
    // Right-side mapping from WorkoutDialog::setupZwiftClick (left unsupported).
    const QVector<QPair<QString,QString>> zwiftRows = {
        {tr("Y"),               tr("Gear up (or difficulty in ERG)")},
        {tr("B"),               tr("Gear down (or difficulty in ERG)")},
        {tr("A"),               tr("Radio: next station")},
        {tr("Z"),               tr("Radio: previous station")},
        {tr("Right paddle (+)"), tr("Start / Pause workout")},
    };
    tabs->addTab(makeTable(zwiftRows), tr("Zwift Click"));

    auto *closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);
}
