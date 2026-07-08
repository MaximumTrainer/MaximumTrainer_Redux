#include "qtmediaplayer.h"

#ifdef GC_HAVE_QTMULTIMEDIA

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QVideoWidget>
#include <QGridLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QLabel>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QSettings>
#include <QDir>
#include <QCursor>
#include <QUrl>
#include <QDebug>

// QtMultimedia-based replacement for the VLC-Qt player. Used on Qt6 (and any
// build without VLC-Qt). Handles the embedded video player and the audio-only
// internet radio behind the same QtMediaPlayer API the rest of the app expects.

QtMediaPlayer::QtMediaPlayer(QWidget *parent) : QWidget(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio  = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);

    // Unlike the short QSoundEffect beeps (which the audio server re-routes to
    // the new default on their own), the radio/media output is a persistent
    // stream pinned to its creation-time device. Follow device changes so it
    // moves to a newly connected output (e.g. headphones) mid-playback.
    m_mediaDevices = new QMediaDevices(this);
    connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged,
            this, &QtMediaPlayer::updateAudioDevice);
    updateAudioDevice();

    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Video surface. Qt's QVideoWidget is a NATIVE surface on Linux/Windows: it
    // paints opaque black over any sibling/child widget and swallows mouse
    // events at the OS level, so a label overlaid on top is invisible and a
    // right-click on it never reaches Qt. The old VLC player avoided this by
    // hiding the placeholder once media loaded; we go one step further and keep
    // the video widget itself HIDDEN until something plays. While idle, the
    // plain container shows the hint label and receives the context-menu click;
    // once media plays the video widget is shown and covers the label.
    m_video = new QVideoWidget(this);
    m_player->setVideoOutput(m_video);
    m_video->hide();
    layout->addWidget(m_video, 0, 0, 1, 1);

    // Placeholder hint shown on the (plain) container until media loads.
    // White text on the always-dark workout view, centred.
    m_hint = new QLabel(tr("Right-click to open a video file or URL"), this);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setStyleSheet("color: white; background-color: transparent; font-size: 16px;");
    m_hint->setAttribute(Qt::WA_TransparentForMouseEvents, true);   // clicks fall through to the container
    layout->addWidget(m_hint, 0, 0, 1, 1);   // same cell, shown while the video is hidden

    // Right-click menu: open media, transport, and volume controls. The player
    // lives in the always-dark WorkoutDialog, so style the menu explicitly dark
    // (otherwise in Light mode it inherits the light palette and renders an
    // out-of-place light menu with poor contrast over the dark video area).
    m_menu = new QMenu(this);
    m_menu->setStyleSheet(
        "QMenu { background-color: #2b2b2b; color: #e0e0e0; border: 1px solid #555; }"
        "QMenu::item:selected { background-color: #4a7ab5; color: white; }"
        "QMenu::separator { height: 1px; background: #555; margin: 4px 0; }");
    m_menu->addAction(tr("Open File…"),  this, &QtMediaPlayer::openLocal);
    m_menu->addAction(tr("Open URL…"),   this, &QtMediaPlayer::openUrl);
    m_menu->addSeparator();
    m_menu->addAction(tr("Play / Pause"), this, &QtMediaPlayer::playOrPause);
    m_menu->addAction(tr("Stop"),         this, &QtMediaPlayer::stop);
    m_menu->addSeparator();
    m_menu->addAction(tr("Volume +10%"), this, [this]() { changeVolume(qMin(100, audioVol + 10)); });
    m_menu->addAction(tr("Volume -10%"), this, [this]() { changeVolume(qMax(0,   audioVol - 10)); });
    m_menu->addAction(tr("Mute / Unmute"), this, [this]() { muteVolume(!isMuted); });

    // Right-click on the container (while idle). Once the video widget is shown
    // it covers the container, so an event filter also forwards its right-clicks
    // (a CustomContextMenu policy on a native QVideoWidget is unreliable).
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &) { videoRightClick(); });
    m_video->installEventFilter(this);

    audioVol = loadSoundVolume();
    applyVolume();

    // Re-emit player state as the legacy playing()/paused()/stopped() signals
    // the config dialog and workout dialog connect to. Show the video widget
    // (hiding the hint) while playing; reveal the hint again when stopped.
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState st) {
        switch (st) {
        case QMediaPlayer::PlayingState:
            if (!isRadio) { m_video->show(); m_hint->hide(); }
            emit playing();
            break;
        case QMediaPlayer::PausedState:
            emit paused();
            break;
        case QMediaPlayer::StoppedState:
            m_video->hide();
            m_hint->show();
            emit stopped();
            break;
        }
    });
}

bool QtMediaPlayer::eventFilter(QObject *watched, QEvent *event)
{
    // Forward right-clicks on the (native) video surface to the context menu,
    // since a CustomContextMenu policy on QVideoWidget does not fire reliably.
    if (watched == m_video && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::RightButton) {
            videoRightClick();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QtMediaPlayer::~QtMediaPlayer() = default;

void QtMediaPlayer::videoRightClick()
{
    m_menu->popup(QCursor::pos());
}

void QtMediaPlayer::openLocal()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Open file"), loadPath(), tr("Multimedia Files (*)"));
    if (file.isEmpty())
        return;

    m_player->setSource(QUrl::fromLocalFile(file));
    m_player->play();
    savePath(file);
}

void QtMediaPlayer::openUrl()
{
    const QString url = QInputDialog::getText(
        this, tr("Open Url"), tr("Enter the URL you want to play"));
    if (url.isEmpty())
        return;

    m_player->setSource(QUrl::fromUserInput(url));
    m_player->play();
}

void QtMediaPlayer::openUrlRadio(QString url)
{
    qDebug() << "QtMultimedia radio open:" << url;
    m_player->setSource(QUrl::fromUserInput(url));
    m_player->play();
}

void QtMediaPlayer::openUrlRadioFromIp(QHostInfo hostInfo)
{
    Q_UNUSED(hostInfo);
}

void QtMediaPlayer::playOrPause()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void QtMediaPlayer::pause()
{
    m_player->pause();
}

void QtMediaPlayer::resume()
{
    m_player->play();
}

void QtMediaPlayer::stop()
{
    m_player->stop();
}

void QtMediaPlayer::changeVolume(int volume)
{
    audioVol = qBound(0, volume, 100);
    applyVolume();
    if (!isRadio)
        saveSoundVolume(audioVol);
}

void QtMediaPlayer::muteVolume(bool mute)
{
    isMuted = mute;
    applyVolume();
}

void QtMediaPlayer::applyVolume()
{
    // QAudioOutput volume is 0.0–1.0; honour the mute flag.
    m_audio->setMuted(isMuted);
    m_audio->setVolume(audioVol / 100.0);
}

void QtMediaPlayer::updateAudioDevice()
{
    const QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (defaultDevice.isNull() || m_audio->device() == defaultDevice)
        return;

    qDebug() << "QtMediaPlayer: switching audio output to" << defaultDevice.description();
    m_audio->setDevice(defaultDevice);
}

// ── QSettings persistence (same keys as the VLC implementation) ──────────────
QString QtMediaPlayer::loadPath()
{
    QSettings settings;
    settings.beginGroup("videoPlayer");
    const QString path = settings.value("loadPath", QDir::homePath()).toString();
    settings.endGroup();
    return path;
}

void QtMediaPlayer::savePath(QString path)
{
    QSettings settings;
    settings.beginGroup("videoPlayer");
    settings.setValue("loadPath", path);
    settings.endGroup();
}

int QtMediaPlayer::loadSoundVolume()
{
    QSettings settings;
    settings.beginGroup("videoPlayer");
    const int volume = settings.value("soundVolume", 100).toInt();
    settings.endGroup();
    return volume;
}

void QtMediaPlayer::saveSoundVolume(int vol)
{
    QSettings settings;
    settings.beginGroup("videoPlayer");
    settings.setValue("soundVolume", vol);
    settings.endGroup();
}

#endif // GC_HAVE_QTMULTIMEDIA
