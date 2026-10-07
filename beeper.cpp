#include "beeper.h"

#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <cstring>

namespace {
const int    TONE_FREQUENCY = 440;  // Hz
const qint16 TONE_AMPLITUDE = 4000; // keep it well below full scale, square waves are loud
const int    BUFFER_MS      = 50;   // small buffer so the beep starts/stops promptly
}

SquareWaveGenerator::SquareWaveGenerator(const QAudioFormat &format, int frequency, QObject *parent)
    : QIODevice(parent)
    , m_format(format)
    , m_halfPeriod(qMax(1, format.sampleRate() / (2 * frequency)))
{
}

qint64 SquareWaveGenerator::bytesAvailable() const
{
    // endless stream
    return m_format.bytesForDuration(BUFFER_MS * 1000) + QIODevice::bytesAvailable();
}

qint64 SquareWaveGenerator::readData(char *data, qint64 maxlen)
{
    const int    channels   = m_format.channelCount();
    const int    frameBytes = m_format.bytesPerFrame();
    const qint64 frames     = maxlen / frameBytes;

    if (m_muted)
    {
        memset(data, 0, frames * frameBytes);
        return frames * frameBytes;
    }

    qint16 *out = reinterpret_cast<qint16 *>(data);
    for (qint64 f = 0; f < frames; ++f)
    {
        qint16 value = ((m_sampleCounter / m_halfPeriod) & 1) ? -TONE_AMPLITUDE : TONE_AMPLITUDE;
        for (int c = 0; c < channels; ++c)
            *out++ = value;
        ++m_sampleCounter;
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

    m_generator = new SquareWaveGenerator(format, TONE_FREQUENCY, this);
    m_generator->open(QIODevice::ReadOnly);

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
    m_generator->setMuted(!on);
}
