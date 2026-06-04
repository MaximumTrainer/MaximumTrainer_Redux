#ifndef APPTHEME_H
#define APPTHEME_H

#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QString>
#include <QColor>
#include <QPalette>

#ifdef Q_OS_WASM
#include <emscripten.h>
#endif

/// Manages the two built-in application stylesheets (Light / Dark)
/// and the automatic "System" mode that tracks the OS colour scheme.
///
/// Usage:
///   AppTheme::apply(qApp, static_cast<AppTheme::Mode>(account->app_theme));
class AppTheme : public QObject
{
    Q_OBJECT
public:
    enum Mode { Light = 0, Dark = 1, System = 2 };

    /// Resolve System mode to the actual Light/Dark mode based on the OS.
    static Mode resolveMode(Mode mode)
    {
        if (mode != System)
            return mode;
#if defined(Q_OS_WASM)
        // In the browser, read the CSS prefers-color-scheme media query.
        // Returns 1 when the user's browser/OS prefers dark, 0 otherwise.
        const int prefersDark = emscripten_run_script_int(
            "(window.matchMedia && "
            "window.matchMedia('(prefers-color-scheme: dark)').matches) ? 1 : 0");
        return prefersDark ? Dark : Light;
#elif QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        return (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)
               ? Dark : Light;
#else
        // Qt < 6.5 has no QStyleHints::colorScheme(). This palette-lightness
        // heuristic does NOT read the GNOME/KDE color-scheme preference under
        // xcb (the palette is the generic light Fusion palette regardless of
        // the desktop's dark-mode setting), so "System" will usually resolve
        // to Light here. Reliable detection requires Qt 6.5+; this is left
        // as-is pending the Qt 6 migration. Users can still pick Dark/Light
        // explicitly in Preferences.
        const QColor bg = QGuiApplication::palette().color(QPalette::Window);
        return (bg.lightness() < 128) ? Dark : Light;
#endif
    }

    /// Apply the stylesheet for the given mode to the application.
    static void apply(QApplication *app, Mode mode)
    {
        const Mode resolved = resolveMode(mode);

        // Force an explicit palette for the chosen mode. Without this the app
        // inherits the OS palette: under an OS dark theme, "Light" mode would
        // render dark/light text from the dark palette (washed-out main window,
        // fully-dark dialogs that set no background of their own), because our
        // stylesheets only set backgrounds for a few named widgets and trust
        // the palette for everything else.
        app->setPalette(resolved == Dark ? darkPalette() : lightPalette());

        if (resolved == Dark) {
            app->setStyleSheet(darkStylesheet());
        } else {
            // Restore the original z_stylesheet-based light theme, plus a few
            // control rules the base sheet lacks (the unstyled native checkbox
            // indicator is nearly invisible against light backgrounds, and
            // menus need an explicit light look for good contrast).
            const QString base = qApp->property("lightStylesheet").toString();
            app->setStyleSheet((base.isEmpty() ? lightStylesheet() : base)
                               + lightControlsStylesheet());
        }
    }

    /// Extra light-mode rules for controls the base light sheet does not style.
    static QString lightControlsStylesheet()
    {
        return QStringLiteral(
"QCheckBox::indicator, QRadioButton::indicator {"
"  width: 14px; height: 14px; border: 1px solid #888; border-radius: 2px; background: #ffffff;"
"}"
"QCheckBox::indicator:checked {"
"  background: #4a7ab5; border-color: #3a6aa5;"
"}"
"QRadioButton::indicator { border-radius: 7px; }"
"QRadioButton::indicator:checked { background: #4a7ab5; border-color: #3a6aa5; }"
"QMenu { background-color: #ffffff; color: #202020; border: 1px solid #b0b0b0; }"
"QMenu::item:selected { background-color: #4a7ab5; color: white; }"
        );
    }

    /// Standard light palette (independent of the OS colour scheme).
    static QPalette lightPalette()
    {
        QPalette p;
        const QColor window(240, 240, 240);
        const QColor base(255, 255, 255);
        const QColor text(20, 20, 20);
        const QColor button(240, 240, 240);
        const QColor highlight(74, 122, 181);
        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, QColor(247, 247, 247));
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, button);
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::ToolTipBase, base);
        p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::PlaceholderText, QColor(120, 120, 120));
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(150, 150, 150));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(150, 150, 150));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(150, 150, 150));
        return p;
    }

    /// Standard dark palette (independent of the OS colour scheme), matched to
    /// the colours used in darkStylesheet().
    static QPalette darkPalette()
    {
        QPalette p;
        const QColor window(43, 43, 43);
        const QColor base(46, 46, 46);
        const QColor text(224, 224, 224);
        const QColor button(61, 61, 61);
        const QColor highlight(74, 122, 181);
        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, QColor(51, 51, 51));
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, button);
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::ToolTipBase, QColor(58, 58, 58));
        p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::PlaceholderText, QColor(140, 140, 140));
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(119, 119, 119));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(119, 119, 119));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(119, 119, 119));
        return p;
    }

    /// The existing (unchanged) light stylesheet.
    static QString lightStylesheet()
    {
        return QStringLiteral(
"QMainWindow#MainWindow {"
"  background: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #2a2a2a, stop:1.0 #4b4b4b);"
"}"
"QLabel#label_headerMain { color: white; }"
"QWidget#widget_fancyMenu {"
"  border: 1px solid #313131;"
"}"
"QWidget#tab_workout1, #tab_workout2, #tab_profile1, #tab_achiev1, #tableView_workout,"
"  #tab_create, #tableView_course, #tab_course, #tab_edit_course {"
"  background-color: #f0f0f0;"
"}"
"QWidget#tabWidget_course, #tab_course, #tab_edit_course {"
"  background-color: #f0f0f0;"
"}"
"QTableView#tableView_workout, QTableView#tableView_course {"
"  gridline-color: gray;"
"}"
"QSplitter::handle {"
"  background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0,"
"    stop:0 rgba(101,104,113,255), stop:0.5 rgba(101,104,113,235), stop:1.0 rgba(101,104,113,255));"
"}"
"QPushButton.boutonLogin {"
"  font: bold; color: white; background-color: #5cb85c;"
"  border: 2px solid #4cae4c; border-radius: 3px; padding: 5px; min-width: 80px;"
"}"
"QPushButton.boutonLogin:hover { background-color: #47a447; border: 2px solid #398439; }"
"QPushButton.boutonLogin:pressed { border: 3px solid #398439; }"
"QLabel.labelImageHr      { image: url(:/image/icon/heart2); }"
"QLabel.labelImagePower   { image: url(:/image/icon/power2); }"
"QLabel.labelImageCadence { image: url(:/image/icon/crank2); }"
"QLabel.labelImageSpeed   { image: url(:/image/icon/speed); }"
"QLabel.labelImageCalories{ image: url(:/image/icon/burn); }"
"QLabel.label_clockgreen  { image: url(:/image/icon/clock_green); }"
"QLabel.label_clockorange { image: url(:/image/icon/clock_orange); }"
        );
    }

    /// Dark variant: darkens the content-area tabs, dialogs, and standard
    /// controls while keeping the existing dark chrome untouched.
    static QString darkStylesheet()
    {
        return QStringLiteral(
// ── Main window chrome (same as light) ──────────────────────────────────
"QMainWindow#MainWindow {"
"  background: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #1a1a1a, stop:1.0 #2e2e2e);"
"}"
"QLabel#label_headerMain { color: white; }"
"QWidget#widget_fancyMenu { border: 1px solid #313131; }"
// ── Content tab areas ────────────────────────────────────────────────────
"QWidget#tab_workout1, #tab_workout2, #tab_profile1, #tab_achiev1, #tableView_workout,"
"  #tab_create, #tableView_course, #tab_course, #tab_edit_course {"
"  background-color: #242424;"
"}"
"QWidget#tabWidget_course, #tab_course, #tab_edit_course {"
"  background-color: #242424;"
"}"
// ── General widget defaults ──────────────────────────────────────────────
"QWidget { background-color: #2b2b2b; color: #e0e0e0; }"
"QDialog { background-color: #2b2b2b; color: #e0e0e0; }"
"QGroupBox { color: #e0e0e0; border: 1px solid #444; border-radius: 4px; margin-top: 8px; padding-top: 4px; }"
"QGroupBox::title { subcontrol-origin: margin; left: 8px; color: #ccc; }"
"QLabel { color: #e0e0e0; background-color: transparent; }"
"QLineEdit, QTextEdit, QPlainTextEdit {"
"  background-color: #3a3a3a; color: #e0e0e0; border: 1px solid #555; border-radius: 3px;"
"}"
"QLineEdit:focus, QTextEdit:focus { border: 1px solid #5a9fd4; }"
"QComboBox {"
"  background-color: #3a3a3a; color: #e0e0e0; border: 1px solid #555; border-radius: 3px; padding: 2px 4px;"
"}"
"QComboBox QAbstractItemView {"
"  background-color: #3a3a3a; color: #e0e0e0; selection-background-color: #4a7ab5;"
"}"
"QSpinBox {"
"  background-color: #3a3a3a; color: #e0e0e0; border: 1px solid #555; border-radius: 3px;"
"}"
"QPushButton {"
"  background-color: #3d3d3d; color: #e0e0e0; border: 1px solid #555; border-radius: 3px; padding: 4px 10px;"
"}"
"QPushButton:hover { background-color: #4a4a4a; }"
"QPushButton:pressed { background-color: #2a2a2a; }"
"QPushButton:disabled { color: #777; border-color: #444; }"
"QPushButton.boutonLogin {"
"  font: bold; color: white; background-color: #4a8f4a;"
"  border: 2px solid #3a7f3a; border-radius: 3px; padding: 5px; min-width: 80px;"
"}"
"QPushButton.boutonLogin:hover { background-color: #3a7f3a; }"
"QCheckBox { color: #e0e0e0; }"
"QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #666; border-radius: 2px; background: #3a3a3a; }"
"QCheckBox::indicator:checked { background: #5a9fd4; border-color: #4a8fc4; }"
"QTabWidget::pane { border: 1px solid #444; background: #2b2b2b; }"
"QTabBar::tab { background: #3a3a3a; color: #ccc; border: 1px solid #444; padding: 6px 12px; }"
"QTabBar::tab:selected { background: #2b2b2b; color: white; border-bottom: none; }"
"QTabBar::tab:hover { background: #444; }"
"QTableView, QListView, QTreeView {"
"  background-color: #2e2e2e; alternate-background-color: #333; color: #e0e0e0;"
"  gridline-color: #444; selection-background-color: #4a7ab5; selection-color: white;"
"}"
"QHeaderView::section {"
"  background-color: #3a3a3a; color: #ccc; border: 1px solid #444; padding: 4px;"
"}"
"QScrollBar:vertical, QScrollBar:horizontal {"
"  background: #2e2e2e; width: 10px; height: 10px; border: none;"
"}"
"QScrollBar::handle:vertical, QScrollBar::handle:horizontal {"
"  background: #555; border-radius: 5px; min-height: 20px;"
"}"
"QScrollBar::handle:hover { background: #777; }"
"QScrollBar::add-line, QScrollBar::sub-line { height: 0px; width: 0px; }"
"QToolTip { background-color: #3a3a3a; color: #e0e0e0; border: 1px solid #555; }"
"QMenuBar { background-color: #2b2b2b; color: #e0e0e0; }"
"QMenuBar::item:selected { background: #3d3d3d; }"
"QMenu { background-color: #2b2b2b; color: #e0e0e0; border: 1px solid #555; }"
"QMenu::item:selected { background: #4a7ab5; }"
"QStatusBar { background-color: #2b2b2b; color: #aaa; }"
"QSplitter::handle { background: #444; }"
"QTableView#tableView_workout, QTableView#tableView_course { gridline-color: #444; }"
"QLabel.labelImageHr      { image: url(:/image/icon/heart2); }"
"QLabel.labelImagePower   { image: url(:/image/icon/power2); }"
"QLabel.labelImageCadence { image: url(:/image/icon/crank2); }"
"QLabel.labelImageSpeed   { image: url(:/image/icon/speed); }"
"QLabel.labelImageCalories{ image: url(:/image/icon/burn); }"
"QLabel.label_clockgreen  { image: url(:/image/icon/clock_green); }"
"QLabel.label_clockorange { image: url(:/image/icon/clock_orange); }"
        );
    }
};

#endif // APPTHEME_H
