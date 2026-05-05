#ifndef SOUNDPLAYER_H
#define SOUNDPLAYER_H

#include <optional>
#include <QResource>
#include <QtCore>

#ifndef Q_OS_WASM
#include "SFML/Audio.hpp"
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
#ifndef Q_OS_WASM
    sf::SoundBuffer bufferSoundAchievement;
    std::optional<sf::Sound> soundAchievement;

    sf::SoundBuffer bufferSoundLastBeepInterval;
    std::optional<sf::Sound> soundLastBeepInterval;

    sf::SoundBuffer bufferSoundFirstBeepInterval;
    std::optional<sf::Sound> soundFirstBeepInterval;

    sf::SoundBuffer bufferSoundEndWorkout;
    std::optional<sf::Sound> soundEndWorkout;

    sf::SoundBuffer bufferSoundStartWorkout;
    std::optional<sf::Sound> soundStartWorkout;

    sf::SoundBuffer bufferSoundCadenceTooLow;
    std::optional<sf::Sound> soundCadenceTooLow;

    sf::SoundBuffer bufferSoundCadenceTooHigh;
    std::optional<sf::Sound> soundCadenceTooHigh;

    sf::SoundBuffer bufferSoundPowerTooLow;
    std::optional<sf::Sound> soundPowerTooLow;

    sf::SoundBuffer bufferSoundPowerTooHigh;
    std::optional<sf::Sound> soundPowerTooHigh;
#endif // Q_OS_WASM

#ifdef Q_OS_WASM
    double m_volume = 0.7; // 0.0 – 1.0, set by setVolume(0–100)
#endif

    /// false when SFML audio is unavailable (no OpenAL device, headless CI).
    /// All play/setVolume calls are no-ops in that case.
    bool m_initialized = false;

};
Q_DECLARE_METATYPE(SoundPlayer*)

#endif // SOUNDPLAYER_H
