#ifndef MYVLCPLAYER_H
#define MYVLCPLAYER_H

#include <QGridLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QSlider>
#include <QMenu>
#include <QEvent>
#include <QHostInfo>

#if defined(GC_HAVE_QTMULTIMEDIA) // ── Native Qt6 QtMultimedia backend ──────

// Embedded media player built on QtMultimedia (QMediaPlayer + QVideoWidget +
// QAudioOutput). Handles both the embedded video player and the audio-only
// internet radio (setRadio(true)). NOTE: the class is still named
// MyVlcPlayer for historical reasons (the .ui files promote `widgetVideo` as
// this class); the VLC-Qt backend has been removed.

QT_FORWARD_DECLARE_CLASS(QMediaPlayer)
QT_FORWARD_DECLARE_CLASS(QAudioOutput)
QT_FORWARD_DECLARE_CLASS(QVideoWidget)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QTimer)

class MyVlcPlayer : public QWidget
{
    Q_OBJECT
public:
    explicit MyVlcPlayer(QWidget *parent = nullptr);
    ~MyVlcPlayer();

    void setMovieTime(int msec);
    void setRadio(bool isRadio) { this->isRadio = isRadio; }

signals:
    void playing();
    void stopped();
    void paused();

public slots:
    void openUrlRadio(QString url);
    void openUrlRadioFromIp(QHostInfo hostInfo);
    void stop();
    void videoRightClick();

    void openLocal();
    void openUrl();
    void playOrPause();
    void pause();
    void resume();

    void resetActionMenu() {}
    void updateSubtitle() {}
    void subtitleChanged() {}

    void changeVolume(int);
    void muteVolume(bool);

    void hideWidgets() {}
    void pushTimerHideWidgets() {}

    void audioTracksChanged(QList<QAction*>) {}
    void checkForSubtitle(QList<QAction*>) {}

private:
    QString loadPath();
    void savePath(QString path);
    int loadSoundVolume();
    void saveSoundVolume(int vol);

    bool isRadio = false;
    int  audioVol = 100;
    bool isMuted = false;

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio  = nullptr;
    QVideoWidget *m_video  = nullptr;
    QMenu        *m_menu   = nullptr;
};

#else // ── No media backend: stub for platforms without VLC-Qt or QtMultimedia ──

class MyVlcPlayer : public QWidget
{
    Q_OBJECT
public:
    explicit MyVlcPlayer(QWidget *parent = nullptr) : QWidget(parent) {}
    ~MyVlcPlayer() {}
    void setMovieTime(int) {}
    void setRadio(bool) {}
signals:
    void playing();
    void stopped();
    void paused();
public slots:
    void openUrlRadio(QString) {}
    void openUrlRadioFromIp(QHostInfo) {}
    void stop() {}
    void videoRightClick() {}
    void openLocal() {}
    void openUrl() {}
    void playOrPause() {}
    void pause() {}
    void resume() {}
    void resetActionMenu() {}
    void updateSubtitle() {}
    void subtitleChanged() {}
    void changeVolume(int) {}
    void muteVolume(bool) {}
    void hideWidgets() {}
    void pushTimerHideWidgets() {}
    void audioTracksChanged(QList<QAction*>) {}
    void checkForSubtitle(QList<QAction*>) {}
};

#endif // media backend

#endif // MYVLCPLAYER_H
