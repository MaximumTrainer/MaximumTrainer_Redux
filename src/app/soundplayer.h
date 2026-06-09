#ifndef SOUNDPLAYER_H
#define SOUNDPLAYER_H

#include <QtCore>

// Desktop sound effects use Qt Multimedia's QSoundEffect (low-latency, WAV).
// WASM has no QtMultimedia and uses the Web Audio stub in soundplayer_wasm.cpp.
// If QtMultimedia is unavailable (GC_HAVE_QTMULTIMEDIA unset) every call is a
// no-op, just like the headless case.
#if !defined(Q_OS_WASM) && defined(GC_HAVE_QTMULTIMEDIA)
#define SOUNDPLAYER_USE_QSOUNDEFFECT
#include <QSoundEffect>
#include <QMediaDevices>
#endif



class SoundPlayer : public QObject
{
    Q_OBJECT

public:
    ~SoundPlayer();
    SoundPlayer(QObject *parent = 0);



    /// 0 to 100
    void setVolume(double volume);

    void playSoundEffectTest();

    void playSoundAchievement();
    void playSoundLastBeepInterval();
    void playSoundFirstBeepInterval();
    void playSoundEndWorkout();
    void playSoundStartWorkout();
    void playSoundPauseWorkout();
    void playSoundCadenceTooLow();
    void playSoundCadenceTooHigh();
    void playSoundPowerTooLow();
    void playSoundPowerTooHigh();




private :
#ifdef SOUNDPLAYER_USE_QSOUNDEFFECT
    // Point every effect at the current default output so beeps follow a newly
    // connected device (e.g. headphones).
    void applyAudioDevice();
    QMediaDevices *m_mediaDevices = nullptr;

    QSoundEffect soundAchievement;
    QSoundEffect soundLastBeepInterval;
    QSoundEffect soundFirstBeepInterval;
    QSoundEffect soundEndWorkout;
    QSoundEffect soundStartWorkout;
    QSoundEffect soundCadenceTooLow;
    QSoundEffect soundCadenceTooHigh;
    QSoundEffect soundPowerTooLow;
    QSoundEffect soundPowerTooHigh;
#endif

#ifdef Q_OS_WASM
    double m_volume = 0.7; // 0.0 – 1.0, set by setVolume(0–100)
#endif

    /// false when sound output is unavailable (no QtMultimedia, headless CI).
    /// All play/setVolume calls are no-ops in that case.
    bool m_initialized = false;

};
Q_DECLARE_METATYPE(SoundPlayer*)

#endif // SOUNDPLAYER_H
