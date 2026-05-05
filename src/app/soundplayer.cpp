#include "soundplayer.h"
#include <QDebug>
#include <sstream>


SoundPlayer::~SoundPlayer() {
    qDebug() << "Destructor SoundPlayer";
}


SoundPlayer::SoundPlayer(QObject *parent) : QObject(parent)
{


    QResource qrcAchievement(":/sound/fireTorch");
    QResource qrcLastBeepInterval(":/sound/secondBeep");
    QResource qrcFirstBeepInterval(":/sound/firstBeep");
    QResource qrcEndWorkout(":/sound/end_workout");
    QResource qrcStartWorkout(":/sound/resume");
    QResource qrcCadenceTooLow(":/sound/cadenceTooLow");
    QResource qrcCadenceTooHigh(":/sound/cadenceTooHigh");
    QResource qrcPowerTooLow(":/sound/powerTooLow");
    QResource qrcPowerTooHigh(":/sound/powerTooHigh");

    // Probe SFML audio availability by redirecting sf::err() to a capture
    // buffer while loading the first sound buffer.  On headless CI runners
    // (no audio device / no OpenAL context) SFML writes an error to sf::err()
    // and alGenSources() returns an invalid source, which later causes
    // alSourcePlay() to crash.  Detecting the failure here lets us disable
    // all sound operations gracefully instead of crashing.
    {
        std::ostringstream probe;
        std::streambuf *const prev = sf::err().rdbuf(probe.rdbuf());
        const bool loaded = bufferSoundAchievement.loadFromMemory(
            qrcAchievement.data(), qrcAchievement.size());
        sf::err().rdbuf(prev);

        if (!loaded || !probe.str().empty()) {
            qWarning() << "SoundPlayer: SFML audio unavailable"
                       << (probe.str().empty()
                               ? QString()
                               : QStringLiteral(" — ") + QString::fromStdString(probe.str()).trimmed())
                       << "; all sound operations disabled.";
            return; // m_initialized stays false
        }
    }

    bufferSoundLastBeepInterval.loadFromMemory(qrcLastBeepInterval.data(), qrcLastBeepInterval.size());
    bufferSoundFirstBeepInterval.loadFromMemory(qrcFirstBeepInterval.data(), qrcFirstBeepInterval.size());
    bufferSoundEndWorkout.loadFromMemory(qrcEndWorkout.data(), qrcEndWorkout.size());
    bufferSoundStartWorkout.loadFromMemory(qrcStartWorkout.data(), qrcStartWorkout.size());
    bufferSoundCadenceTooLow.loadFromMemory(qrcCadenceTooLow.data(), qrcCadenceTooLow.size());
    bufferSoundCadenceTooHigh.loadFromMemory(qrcCadenceTooHigh.data(), qrcCadenceTooHigh.size());
    bufferSoundPowerTooLow.loadFromMemory(qrcPowerTooLow.data(), qrcPowerTooLow.size());
    bufferSoundPowerTooHigh.loadFromMemory(qrcPowerTooHigh.data(), qrcPowerTooHigh.size());

    soundAchievement.emplace(bufferSoundAchievement);
    soundLastBeepInterval.emplace(bufferSoundLastBeepInterval);
    soundFirstBeepInterval.emplace(bufferSoundFirstBeepInterval);
    soundEndWorkout.emplace(bufferSoundEndWorkout);
    soundStartWorkout.emplace(bufferSoundStartWorkout);
    soundCadenceTooLow.emplace(bufferSoundCadenceTooLow);
    soundCadenceTooHigh.emplace(bufferSoundCadenceTooHigh);
    soundPowerTooLow.emplace(bufferSoundPowerTooLow);
    soundPowerTooHigh.emplace(bufferSoundPowerTooHigh);

    m_initialized = true;

    setVolume(100);
}


//------------------------------------------------------------------------------------------------------------------------
void SoundPlayer::setVolume(double volume) {

    qDebug() << "setVolume soundPlayer";

    if (!m_initialized) return;
    soundAchievement->setVolume(volume);
    soundLastBeepInterval->setVolume(volume);
    soundFirstBeepInterval->setVolume(volume);
    soundEndWorkout->setVolume(volume);
    soundStartWorkout->setVolume(volume);
    soundCadenceTooLow->setVolume(volume);
    soundCadenceTooHigh->setVolume(volume);
    soundPowerTooLow->setVolume(volume);
    soundPowerTooHigh->setVolume(volume);
}


void SoundPlayer::playSoundEffectTest() {
    if (!m_initialized) return;
    soundFirstBeepInterval->play();
}
void SoundPlayer::playSoundAchievement() {
    if (!m_initialized) return;
    soundAchievement->play();
}


void SoundPlayer::playSoundLastBeepInterval() {
    if (!m_initialized) return;
    soundLastBeepInterval->play();
}
void SoundPlayer::playSoundFirstBeepInterval() {
    if (!m_initialized) return;
    soundFirstBeepInterval->play();
}
void SoundPlayer::playSoundEndWorkout() {
    if (!m_initialized) return;
    soundEndWorkout->play();
}
void SoundPlayer::playSoundStartWorkout() {
    if (!m_initialized) return;
    soundStartWorkout->play();
}
void SoundPlayer::playSoundPauseWorkout() {
    if (!m_initialized) return;
    soundStartWorkout->play();
}
void SoundPlayer::playSoundCadenceTooLow() {
    if (!m_initialized) return;
    soundCadenceTooLow->play();
}
void SoundPlayer::playSoundCadenceTooHigh() {
    if (!m_initialized) return;
    soundCadenceTooHigh->play();
}
void SoundPlayer::playSoundPowerTooLow() {
    if (!m_initialized) return;
    soundPowerTooLow->play();
}
void SoundPlayer::playSoundPowerTooHigh() {
    if (!m_initialized) return;
    soundPowerTooHigh->play();
}
