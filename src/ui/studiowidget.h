#ifndef STUDIOWIDGET_H
#define STUDIOWIDGET_H

#include <QWidget>

class QCheckBox;
class QSpinBox;
class Account;

/*
 * StudioWidget
 *
 * Main-window page (a FancyTabBar tab) for Studio mode. Replaces the former
 * server-hosted QWebEngineView studio page (now dead). Built programmatically
 * like SensorsWidget. Exposes the two settings that used to live in the
 * in-workout "Workout Config" dialog: enable Studio mode and the rider count.
 *
 * It does not own the studio-mode side effects (window title, History-tab
 * enable/disable) — those stay in MainWindow, reached via the signals below.
 */
class StudioWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StudioWidget(QWidget *parent = nullptr);

    /// Re-read persisted settings into the controls (e.g. when the tab is shown).
    void reload();

signals:
    void studioModeChanged(bool enabled);
    void riderCountChanged(int nbRiders);

private slots:
    void onStudioModeToggled(bool enabled);
    void onRiderCountChanged(int nbRiders);

private:
    void buildUi();

    QCheckBox *m_enableCheck   = nullptr;
    QSpinBox  *m_riderCountSpin = nullptr;
    Account   *m_account = nullptr;
};

#endif // STUDIOWIDGET_H
