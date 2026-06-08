#ifndef QTMEDIAPLAYER_H
#define QTMEDIAPLAYER_H

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
// internet radio (setRadio(true)). Replaces the former VLC-Qt backend.

QT_FORWARD_DECLARE_CLASS(QMediaPlayer)
QT_FORWARD_DECLARE_CLASS(QAudioOutput)
QT_FORWARD_DECLARE_CLASS(QVideoWidget)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QTimer)

class QtMediaPlayer : public QWidget
{
    Q_OBJECT
public:
    explicit QtMediaPlayer(QWidget *parent = nullptr);
    ~QtMediaPlayer();

    void setMovieTime(int msec);
    void setRadio(bool isRadio) { this->isRadio = isRadio; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

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

    void applyVolume();

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio  = nullptr;
    QVideoWidget *m_video  = nullptr;
    QMenu        *m_menu   = nullptr;
    QLabel       *m_hint   = nullptr;   // "right-click to open media" overlay
};

#else // ── No media backend: stub for platforms without VLC-Qt or QtMultimedia ──

class QtMediaPlayer : public QWidget
{
    Q_OBJECT
public:
    explicit QtMediaPlayer(QWidget *parent = nullptr) : QWidget(parent) {}
    ~QtMediaPlayer() {}
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

#endif // QTMEDIAPLAYER_H
