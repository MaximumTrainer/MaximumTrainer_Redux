#include "myvlcplayer.h"

#ifdef GC_HAVE_QTMULTIMEDIA

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QGridLayout>
#include <QMenu>
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
// internet radio behind the same MyVlcPlayer API the rest of the app expects.

MyVlcPlayer::MyVlcPlayer(QWidget *parent) : QWidget(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio  = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);

    // Video surface (unused/harmless in radio mode).
    m_video = new QVideoWidget(this);
    m_player->setVideoOutput(m_video);

    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_video, 0, 0, 1, 1);

    // Placeholder hint shown over the (empty) video area until media loads.
    // White text on the always-dark workout view, centred.
    m_hint = new QLabel(tr("Right-click to open a video file or URL"), this);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setStyleSheet("color: white; background-color: transparent; font-size: 16px;");
    m_hint->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    layout->addWidget(m_hint, 0, 0, 1, 1);   // same cell, overlays the video

    // Right-click menu: open media, transport, and volume controls.
    m_menu = new QMenu(this);
    m_menu->addAction(tr("Open File…"),  this, &MyVlcPlayer::openLocal);
    m_menu->addAction(tr("Open URL…"),   this, &MyVlcPlayer::openUrl);
    m_menu->addSeparator();
    m_menu->addAction(tr("Play / Pause"), this, &MyVlcPlayer::playOrPause);
    m_menu->addAction(tr("Stop"),         this, &MyVlcPlayer::stop);
    m_menu->addSeparator();
    m_menu->addAction(tr("Volume +10%"), this, [this]() { changeVolume(qMin(100, audioVol + 10)); });
    m_menu->addAction(tr("Volume -10%"), this, [this]() { changeVolume(qMax(0,   audioVol - 10)); });
    m_menu->addAction(tr("Mute / Unmute"), this, [this]() { muteVolume(!isMuted); });
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &) { videoRightClick(); });

    audioVol = loadSoundVolume();
    applyVolume();

    // Re-emit player state as the legacy playing()/paused()/stopped() signals
    // the config dialog and workout dialog connect to. Also hide the hint once
    // something is playing.
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState st) {
        switch (st) {
        case QMediaPlayer::PlayingState: m_hint->hide(); emit playing(); break;
        case QMediaPlayer::PausedState:  emit paused();  break;
        case QMediaPlayer::StoppedState: emit stopped(); break;
        }
    });
}

MyVlcPlayer::~MyVlcPlayer() = default;

void MyVlcPlayer::setMovieTime(int msec)
{
    m_player->setPosition(msec);
}

void MyVlcPlayer::videoRightClick()
{
    m_menu->popup(QCursor::pos());
}

void MyVlcPlayer::openLocal()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Open file"), loadPath(), tr("Multimedia Files (*)"));
    if (file.isEmpty())
        return;

    m_player->setSource(QUrl::fromLocalFile(file));
    m_player->play();
    savePath(file);
}

void MyVlcPlayer::openUrl()
{
    const QString url = QInputDialog::getText(
        this, tr("Open Url"), tr("Enter the URL you want to play"));
    if (url.isEmpty())
        return;

    m_player->setSource(QUrl::fromUserInput(url));
    m_player->play();
}

void MyVlcPlayer::openUrlRadio(QString url)
{
    qDebug() << "QtMultimedia radio open:" << url;
    m_player->setSource(QUrl::fromUserInput(url));
    m_player->play();
}

void MyVlcPlayer::openUrlRadioFromIp(QHostInfo hostInfo)
{
    Q_UNUSED(hostInfo);
}

void MyVlcPlayer::playOrPause()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void MyVlcPlayer::pause()
{
    m_player->pause();
}

void MyVlcPlayer::resume()
{
    m_player->play();
}

void MyVlcPlayer::stop()
{
    m_player->stop();
}

void MyVlcPlayer::changeVolume(int volume)
{
    audioVol = qBound(0, volume, 100);
    applyVolume();
    if (!isRadio)
        saveSoundVolume(audioVol);
}

void MyVlcPlayer::muteVolume(bool mute)
{
    isMuted = mute;
    applyVolume();
}

void MyVlcPlayer::applyVolume()
{
    // QAudioOutput volume is 0.0–1.0; honour the mute flag.
    m_audio->setMuted(isMuted);
    m_audio->setVolume(audioVol / 100.0);
}

// ── QSettings persistence (same keys as the VLC implementation) ──────────────
QString MyVlcPlayer::loadPath()
{
    QSettings settings;
    settings.beginGroup("videoPlayer");
    const QString path = settings.value("loadPath", QDir::homePath()).toString();
    settings.endGroup();
    return path;
}

void MyVlcPlayer::savePath(QString path)
{
    QSettings settings;
    settings.beginGroup("videoPlayer");
    settings.setValue("loadPath", path);
    settings.endGroup();
}

int MyVlcPlayer::loadSoundVolume()
{
    QSettings settings;
    settings.beginGroup("videoPlayer");
    const int volume = settings.value("soundVolume", 100).toInt();
    settings.endGroup();
    return volume;
}

void MyVlcPlayer::saveSoundVolume(int vol)
{
    QSettings settings;
    settings.beginGroup("videoPlayer");
    settings.setValue("soundVolume", vol);
    settings.endGroup();
}

#endif // GC_HAVE_QTMULTIMEDIA
