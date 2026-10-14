#ifndef BEEPER_H
#define BEEPER_H

#include <QObject>
#include <QIODevice>
#include <QAudioFormat>
#include <mutex>

class QAudioSink;

// Endless tone fed to the audio output: a square wave (the classic CHIP-8
// buzzer) or, once set, an XO-CHIP audio pattern. Produces silence while off,
// so the audio stream can keep running and the tone starts without delay.
class ToneGenerator : public QIODevice
{
    Q_OBJECT
public:
    static const int PATTERN_BYTES = 16; // 128 one-bit samples

    ToneGenerator(const QAudioFormat &format, int frequency, QObject *parent = nullptr);

    // Switches the tone on or off. Once switched on, it plays for at least one
    // 60 Hz frame even if it is switched off sooner: the audio device pulls
    // samples every 20-30 ms, so a shorter on-time could otherwise fall between
    // two pulls and never be heard. CHIP-8 programs often beep for one frame.
    void setOn(bool on);
    // Plays the 128-bit pattern on a loop at 4000 * 2^((pitch - 64) / 48) bits
    // per second. A null pattern goes back to the square wave.
    void setPattern(const uint8_t *pattern, uint8_t pitch);

    bool   isSequential() const override { return true; }
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QAudioFormat m_format;
    qint64       m_halfPeriod; // samples per half period of the square wave
    qint64       m_sampleCounter = 0;

    // readData() runs on the audio thread, so everything below is guarded by a mutex
    std::mutex m_mutex;
    bool       m_on             = false;
    qint64     m_minSamplesLeft = 0; // tone still owed after switching off (see setOn())
    bool       m_usePattern     = false;
    uint8_t    m_pattern[PATTERN_BYTES] = {};
    double     m_patternStep = 0; // pattern bits per output sample
    double     m_patternPos  = 0; // current position in the pattern, in bits
};

// Plays the CHIP-8 buzzer tone, or the XO-CHIP audio pattern, while active.
class Beeper : public QObject
{
    Q_OBJECT
public:
    explicit Beeper(QObject *parent = nullptr);
    ~Beeper() override;

    void setActive(bool on);
    // See ToneGenerator::setPattern().
    void setPattern(const uint8_t *pattern, uint8_t pitch);

private:
    QAudioSink    *m_sink      = nullptr;
    ToneGenerator *m_generator = nullptr;
    bool           m_active    = false;
};

#endif // BEEPER_H
