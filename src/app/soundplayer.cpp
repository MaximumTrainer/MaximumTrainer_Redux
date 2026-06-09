#include "soundplayer.h"
#include <QDebug>
#ifdef SOUNDPLAYER_USE_QSOUNDEFFECT
#include <QAudioDevice>
#endif


SoundPlayer::~SoundPlayer() {
    qDebug() << "Destructor SoundPlayer";
}


#ifdef SOUNDPLAYER_USE_QSOUNDEFFECT

SoundPlayer::SoundPlayer(QObject *parent) : QObject(parent)
{
    soundAchievement.setSource(QUrl("qrc:/sound/fireTorch"));
    soundLastBeepInterval.setSource(QUrl("qrc:/sound/secondBeep"));
    soundFirstBeepInterval.setSource(QUrl("qrc:/sound/firstBeep"));
    soundEndWorkout.setSource(QUrl("qrc:/sound/end_workout"));
    soundStartWorkout.setSource(QUrl("qrc:/sound/resume"));
    soundCadenceTooLow.setSource(QUrl("qrc:/sound/cadenceTooLow"));
    soundCadenceTooHigh.setSource(QUrl("qrc:/sound/cadenceTooHigh"));
    soundPowerTooLow.setSource(QUrl("qrc:/sound/powerTooLow"));
    soundPowerTooHigh.setSource(QUrl("qrc:/sound/powerTooHigh"));

    // QSoundEffect binds to the default device at creation; follow later changes
    // (e.g. plugging in headphones) so beeps move to the new output.
    m_mediaDevices = new QMediaDevices(this);
    connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged,
            this, &SoundPlayer::applyAudioDevice);
    applyAudioDevice();

    m_initialized = true;
    setVolume(100);
}


//------------------------------------------------------------------------------------------------------------------------
void SoundPlayer::applyAudioDevice() {

    const QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (defaultDevice.isNull())
        return;

    for (QSoundEffect *effect : { &soundAchievement, &soundLastBeepInterval,
                                  &soundFirstBeepInterval, &soundEndWorkout,
                                  &soundStartWorkout, &soundCadenceTooLow,
                                  &soundCadenceTooHigh, &soundPowerTooLow,
                                  &soundPowerTooHigh }) {
        if (effect->audioDevice() != defaultDevice)
            effect->setAudioDevice(defaultDevice);
    }
}


//------------------------------------------------------------------------------------------------------------------------
void SoundPlayer::setVolume(double volume) {

    qDebug() << "setVolume soundPlayer";

    if (!m_initialized) return;
    // Callers pass 0–100; QSoundEffect expects a linear 0.0–1.0.
    const qreal v = qBound(0.0, volume / 100.0, 1.0);
    soundAchievement.setVolume(v);
    soundLastBeepInterval.setVolume(v);
    soundFirstBeepInterval.setVolume(v);
    soundEndWorkout.setVolume(v);
    soundStartWorkout.setVolume(v);
    soundCadenceTooLow.setVolume(v);
    soundCadenceTooHigh.setVolume(v);
    soundPowerTooLow.setVolume(v);
    soundPowerTooHigh.setVolume(v);
}


void SoundPlayer::playSoundEffectTest() {
    if (!m_initialized) return;
    soundFirstBeepInterval.play();
}
void SoundPlayer::playSoundAchievement() {
    if (!m_initialized) return;
    soundAchievement.play();
}


void SoundPlayer::playSoundLastBeepInterval() {
    if (!m_initialized) return;
    soundLastBeepInterval.play();
}
void SoundPlayer::playSoundFirstBeepInterval() {
    if (!m_initialized) return;
    soundFirstBeepInterval.play();
}
void SoundPlayer::playSoundEndWorkout() {
    if (!m_initialized) return;
    soundEndWorkout.play();
}
void SoundPlayer::playSoundStartWorkout() {
    if (!m_initialized) return;
    soundStartWorkout.play();
}
void SoundPlayer::playSoundPauseWorkout() {
    if (!m_initialized) return;
    soundStartWorkout.play();
}
void SoundPlayer::playSoundCadenceTooLow() {
    if (!m_initialized) return;
    soundCadenceTooLow.play();
}
void SoundPlayer::playSoundCadenceTooHigh() {
    if (!m_initialized) return;
    soundCadenceTooHigh.play();
}
void SoundPlayer::playSoundPowerTooLow() {
    if (!m_initialized) return;
    soundPowerTooLow.play();
}
void SoundPlayer::playSoundPowerTooHigh() {
    if (!m_initialized) return;
    soundPowerTooHigh.play();
}

#else // !SOUNDPLAYER_USE_QSOUNDEFFECT

// QtMultimedia unavailable: every operation is a silent no-op.
SoundPlayer::SoundPlayer(QObject *parent) : QObject(parent) {
    qWarning() << "SoundPlayer: QtMultimedia unavailable; all sound operations disabled.";
}

void SoundPlayer::setVolume(double) {}
void SoundPlayer::playSoundEffectTest() {}
void SoundPlayer::playSoundAchievement() {}
void SoundPlayer::playSoundLastBeepInterval() {}
void SoundPlayer::playSoundFirstBeepInterval() {}
void SoundPlayer::playSoundEndWorkout() {}
void SoundPlayer::playSoundStartWorkout() {}
void SoundPlayer::playSoundPauseWorkout() {}
void SoundPlayer::playSoundCadenceTooLow() {}
void SoundPlayer::playSoundCadenceTooHigh() {}
void SoundPlayer::playSoundPowerTooLow() {}
void SoundPlayer::playSoundPowerTooHigh() {}

#endif // SOUNDPLAYER_USE_QSOUNDEFFECT
