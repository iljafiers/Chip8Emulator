#ifndef BEEPER_H
#define BEEPER_H

#include <QObject>
#include <QIODevice>
#include <QAudioFormat>
#include <atomic>

class QAudioSink;

// Endless square wave fed to the audio output. Produces silence while muted,
// so the audio stream can keep running and the beep starts without delay.
class SquareWaveGenerator : public QIODevice
{
    Q_OBJECT
public:
    SquareWaveGenerator(const QAudioFormat &format, int frequency, QObject *parent = nullptr);

    void setMuted(bool muted) { m_muted = muted; }

    bool   isSequential() const override { return true; }
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QAudioFormat      m_format;
    qint64            m_halfPeriod; // samples per half period of the square wave
    qint64            m_sampleCounter = 0;
    std::atomic<bool> m_muted{true};
};

// Plays the CHIP-8 buzzer tone while active.
class Beeper : public QObject
{
    Q_OBJECT
public:
    explicit Beeper(QObject *parent = nullptr);
    ~Beeper() override;

    void setActive(bool on);

private:
    QAudioSink          *m_sink      = nullptr;
    SquareWaveGenerator *m_generator = nullptr;
    bool                 m_active    = false;
};

#endif // BEEPER_H
