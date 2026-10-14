#include "beeper.h"

#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <cmath>
#include <cstring>

namespace {
const int    TONE_FREQUENCY = 440;  // Hz
const qint16 TONE_AMPLITUDE = 4000; // keep it well below full scale, square waves are loud
const int    BUFFER_MS      = 50;   // small buffer so the beep starts/stops promptly
}

ToneGenerator::ToneGenerator(const QAudioFormat &format, int frequency, QObject *parent)
    : QIODevice(parent)
    , m_format(format)
    , m_halfPeriod(qMax(1, format.sampleRate() / (2 * frequency)))
{
}

void ToneGenerator::setOn(bool on)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (on && !m_on)
        m_minSamplesLeft = m_format.sampleRate() / 60;
    m_on = on;
}

void ToneGenerator::setPattern(const uint8_t *pattern, uint8_t pitch)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_usePattern = pattern != nullptr;
    if (!pattern)
        return;
    memcpy(m_pattern, pattern, PATTERN_BYTES);
    const double bitsPerSecond = 4000.0 * std::pow(2.0, (pitch - 64) / 48.0);
    m_patternStep              = bitsPerSecond / m_format.sampleRate();
}

qint64 ToneGenerator::bytesAvailable() const
{
    // endless stream
    return m_format.bytesForDuration(BUFFER_MS * 1000) + QIODevice::bytesAvailable();
}

qint64 ToneGenerator::readData(char *data, qint64 maxlen)
{
    const int    channels   = m_format.channelCount();
    const int    frameBytes = m_format.bytesPerFrame();
    const qint64 frames     = maxlen / frameBytes;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_on && m_minSamplesLeft == 0)
    {
        memset(data, 0, frames * frameBytes);
        return frames * frameBytes;
    }

    qint16 *out = reinterpret_cast<qint16 *>(data);
    for (qint64 f = 0; f < frames; ++f)
    {
        if (m_minSamplesLeft > 0)
            --m_minSamplesLeft;
        else if (!m_on)
        {
            // switched off and the minimum has been played: silence for the rest
            memset(out, 0, (frames - f) * frameBytes);
            break;
        }

        bool high;
        if (m_usePattern)
        {
            // bits play most significant first, byte by byte
            const int bit = static_cast<int>(m_patternPos);
            high          = m_pattern[bit / 8] & (0x80 >> (bit % 8));
            m_patternPos  = std::fmod(m_patternPos + m_patternStep, PATTERN_BYTES * 8);
        }
        else
        {
            high = !((m_sampleCounter / m_halfPeriod) & 1);
            ++m_sampleCounter;
        }

        const qint16 value = high ? TONE_AMPLITUDE : -TONE_AMPLITUDE;
        for (int c = 0; c < channels; ++c)
            *out++ = value;
    }
    return frames * frameBytes;
}
Beeper::Beeper(QObject *parent)
    : QObject(parent)
{
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull())
        return; // no audio output, stay silent

    QAudioFormat format = device.preferredFormat();
    format.setSampleFormat(QAudioFormat::Int16);
    if (!device.isFormatSupported(format))
    {
        format.setSampleRate(44100);
        format.setChannelCount(1);
        if (!device.isFormatSupported(format))
            return;
    }

    m_generator = new ToneGenerator(format, TONE_FREQUENCY, this);
    // unbuffered, so QIODevice doesn't read ahead: mute and pattern changes are heard at once
    m_generator->open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    m_sink = new QAudioSink(device, format, this);
    m_sink->setBufferSize(format.bytesForDuration(BUFFER_MS * 1000));
    m_sink->start(m_generator);
}

Beeper::~Beeper()
{
    if (m_sink)
        m_sink->stop();
}

void Beeper::setActive(bool on)
{
    if (on == m_active || !m_generator)
        return;
    m_active = on;
    m_generator->setOn(on);
}

void Beeper::setPattern(const uint8_t *pattern, uint8_t pitch)
{
    if (m_generator)
        m_generator->setPattern(pattern, pitch);
}
