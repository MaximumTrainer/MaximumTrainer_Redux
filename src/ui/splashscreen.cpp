#include "splashscreen.h"

#include <QApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QScreen>

// ── Splash dimensions ────────────────────────────────────────────────────────
static constexpr int kSplashWidth  = 600;
static constexpr int kSplashHeight = 380;

// ── Progress bar (pinned to the very bottom) ─────────────────────────────────
static constexpr int kBarHeight = 5;

// ── Status label (sits just above the progress bar) ─────────────────────────
static constexpr int kStatusHeight        = 24;
static constexpr int kStatusMarginBottom  = 8;   // gap between label and bar

// ── "REDUX" tag drawn directly in drawContents() ────────────────────────────
static constexpr int kReduxY = 260;   // vertical position of the tag text

// ── Neon green used for progress bar chunk ───────────────────────────────────
static const QColor kNeonGreen { 0x39, 0xFF, 0x14 };  // #39FF14


// ─────────────────────────────────────────────────────────────────────────────
// Static helper: build the background pixmap
// ─────────────────────────────────────────────────────────────────────────────
QPixmap SplashScreen::createBackground()
{
    QPixmap pix(kSplashWidth, kSplashHeight);
    pix.fill(Qt::black);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Dark background gradient (top-to-bottom)
    QLinearGradient bg(0, 0, 0, kSplashHeight);
    bg.setColorAt(0.0, QColor("#1e1e1e"));
    bg.setColorAt(1.0, QColor("#0a0a0a"));
    p.fillRect(0, 0, kSplashWidth, kSplashHeight, bg);

    // Power-zone accent strip across the very top (3 px)
    // Colours mirror classic power-zone palette: red → orange → yellow → green
    QLinearGradient accent(0, 0, kSplashWidth, 0);
    accent.setColorAt(0.00, QColor("#cc0000"));
    accent.setColorAt(0.35, QColor("#ff6600"));
    accent.setColorAt(0.65, QColor("#ffcc00"));
    accent.setColorAt(1.00, kNeonGreen);
    p.fillRect(0, 0, kSplashWidth, 3, accent);

    // Application logo — centred horizontally, positioned in the upper half
    QPixmap logo(":/image/icon/logo");
    if (!logo.isNull()) {
        QPixmap scaled = logo.scaled(
            240, 130,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        int logoX = (kSplashWidth  - scaled.width())  / 2;
        int logoY = (kSplashHeight - scaled.height())  / 2 - 40;
        p.drawPixmap(logoX, logoY, scaled);
    }

    p.end();
    return pix;
}


// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
SplashScreen::SplashScreen()
    : QSplashScreen(createBackground())
{
    // ── Status label ─────────────────────────────────────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setText(tr("Initializing…"));
    m_statusLabel->setGeometry(
        0,
        kSplashHeight - kBarHeight - kStatusMarginBottom - kStatusHeight,
        kSplashWidth,
        kStatusHeight);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "  color: rgba(200, 200, 200, 210);"
        "  font-size: 11px;"
        "  font-family: 'Segoe UI', 'Roboto', Arial, sans-serif;"
        "  letter-spacing: 1px;"
        "  background: transparent;"
        "}");

    // ── Neon-green progress bar (5 px, pinned to bottom) ─────────────────────
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(kBarHeight);
    m_progressBar->setGeometry(0, kSplashHeight - kBarHeight,
                               kSplashWidth, kBarHeight);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  background-color: #111111;"
        "  border: none;"
        "  border-radius: 0px;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: qlineargradient("
        "      x1:0, y1:0, x2:1, y2:0,"
        "      stop:0   #00cc00,"
        "      stop:0.5 #39FF14,"
        "      stop:1.0 #00FF41);"
        "}");

    // ── Centre on the primary screen ─────────────────────────────────────────
    if (QScreen *screen = QApplication::primaryScreen()) {
        QRect sg = screen->availableGeometry();
        move(sg.center() - rect().center());
    }
}


bool SplashScreen::event(QEvent *event)
{
    // QSplashScreen handles Show by waiting up to 1 s for its window to become
    // visible, but Show is delivered before the window is made visible, so the
    // wait always runs out and delays every launch by a full second. Startup
    // pumps events after show() and on every progress update, which paints it.
    if (event->type() == QEvent::Show)
        return QWidget::event(event);
    return QSplashScreen::event(event);
}


// ─────────────────────────────────────────────────────────────────────────────
// Public slots
// ─────────────────────────────────────────────────────────────────────────────
void SplashScreen::setProgress(int value)
{
    m_progressBar->setValue(value);
    m_progressBar->repaint();
    // Exclude user-input events to prevent re-entrancy during startup.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void SplashScreen::setStatusMessage(const QString &message)
{
    m_statusLabel->setText(message);
    m_statusLabel->repaint();
    // Exclude user-input events to prevent re-entrancy during startup.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}


// ─────────────────────────────────────────────────────────────────────────────
// drawContents — paint "REDUX" edition tag on top of the background pixmap
// ─────────────────────────────────────────────────────────────────────────────
void SplashScreen::drawContents(QPainter *painter)
{
    // Let the base class draw the pixmap first
    QSplashScreen::drawContents(painter);

    // "REDUX" tag: thin, spaced capitals in neon green
    QFont reduxFont;
    reduxFont.setFamily("Segoe UI");
    reduxFont.setPointSize(9);
    reduxFont.setWeight(QFont::Light);
    reduxFont.setLetterSpacing(QFont::AbsoluteSpacing, 4.0);

    QColor reduxColor = kNeonGreen;
    reduxColor.setAlpha(200);
    painter->setFont(reduxFont);
    painter->setPen(reduxColor);
    painter->drawText(
        QRect(0, kReduxY, kSplashWidth, 20),
        Qt::AlignCenter,
        "REDUX");
}
