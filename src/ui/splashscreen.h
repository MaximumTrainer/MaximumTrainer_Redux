#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QSplashScreen>
#include <QProgressBar>
#include <QLabel>

///
/// \brief Custom startup splash screen for MaximumTrainer.
///
/// Displays a dark-themed, full-image splash window while the application
/// initialises.  The screen features:
///   - A programmatic dark-gradient background with the application logo
///   - A power-zone accent strip across the top (red → orange → yellow → green)
///   - A "REDUX" edition tag drawn directly on the pixmap
///   - A \c QLabel that surfaces the current initialisation step to the user
///   - A 5 px neon-green \c QProgressBar pinned to the very bottom edge
///
/// Typical usage in \c main():
/// \code
///   SplashScreen splash;
///   splash.show();
///   QApplication::processEvents();
///
///   splash.setStatusMessage(tr("Loading configuration…"));
///   splash.setProgress(20);
///   // … initialisation work …
///   splash.finish(&mainWindow);
/// \endcode
///
class SplashScreen : public QSplashScreen
{
    Q_OBJECT

public:
    explicit SplashScreen();
    ~SplashScreen() override = default;

    /// Update the neon-green progress bar (0–100).
    void setProgress(int value);

    /// Update the status text shown above the progress bar.
    void setStatusMessage(const QString &message);

protected:
    void drawContents(QPainter *painter) override;
    bool event(QEvent *event) override;

private:
    QProgressBar *m_progressBar { nullptr };
    QLabel       *m_statusLabel  { nullptr };

    /// Build the 600×380 background pixmap (gradient + logo).
    static QPixmap createBackground();
};

#endif // SPLASHSCREEN_H
