#include "displaywidget.h"

#include <QPainter>
#include <cstring>
#include <algorithm>

const QList<DisplayThemeInfo> &displayThemes()
{
    static const QList<DisplayThemeInfo> themes = {
        {DisplayTheme::GreenOnBlack, "greenOnBlack", QT_TRANSLATE_NOOP("DisplayWidget", "Green on Black"),
         QColor(0, 220, 90), Qt::black},
        {DisplayTheme::OrangeOnBlack, "orangeOnBlack", QT_TRANSLATE_NOOP("DisplayWidget", "Orange on Black"),
         QColor(255, 140, 0), Qt::black},
        {DisplayTheme::WhiteOnBlack, "whiteOnBlack", QT_TRANSLATE_NOOP("DisplayWidget", "White on Black"),
         Qt::white, Qt::black},
        {DisplayTheme::BlackOnWhite, "blackOnWhite", QT_TRANSLATE_NOOP("DisplayWidget", "Black on White"),
         Qt::black, Qt::white},
    };
    return themes;
}

const DisplayThemeInfo &displayThemeInfo(DisplayTheme theme)
{
    for (const DisplayThemeInfo &info : displayThemes())
    {
        if (info.theme == theme)
            return info;
    }
    return displayThemes().first();
}

DisplayTheme displayThemeFromKey(const QString &key, DisplayTheme fallback)
{
    for (const DisplayThemeInfo &info : displayThemes())
    {
        if (key == QLatin1String(info.key))
            return info.theme;
    }
    return fallback;
}

DisplayWidget::DisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    memset(m_buffer, 0, sizeof(m_buffer));
    setMinimumSize(Chip8::LORES_WIDTH * 4, Chip8::LORES_HEIGHT * 4);
    setAutoFillBackground(true);
    setTheme(DisplayTheme::OrangeOnBlack);
}

void DisplayWidget::setBuffer(const uint8_t *src, int width, int height)
{
    m_gridWidth  = width;
    m_gridHeight = height;
    memcpy(m_buffer, src, static_cast<size_t>(width * height));
    update();
}

void DisplayWidget::setTheme(DisplayTheme theme)
{
    const DisplayThemeInfo &info = displayThemeInfo(theme);
    m_foreground                 = info.foreground;
    m_background                 = info.background;

    // also used for the margins around the image
    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_background);
    setPalette(pal);
    update();
}

int DisplayWidget::pixelScale() const
{
    const qreal dpr    = devicePixelRatioF();
    int         scaleX = static_cast<int>(width() * dpr) / m_gridWidth;
    int         scaleY = static_cast<int>(height() * dpr) / m_gridHeight;
    return std::max(1, std::min(scaleX, scaleY));
}

QSize DisplayWidget::sizeHint() const
{
    return QSize(Chip8::LORES_WIDTH * 10, Chip8::LORES_HEIGHT * 10);
}

void DisplayWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), m_background);

    // draw in device pixels, so pixel edges land on whole screen pixels
    const qreal dpr = devicePixelRatioF();
    painter.scale(1.0 / dpr, 1.0 / dpr);
    const int deviceWidth  = static_cast<int>(width() * dpr);
    const int deviceHeight = static_cast<int>(height() * dpr);

    int scale   = pixelScale();
    int offsetX = (deviceWidth - scale * m_gridWidth) / 2;
    int offsetY = (deviceHeight - scale * m_gridHeight) / 2;

    painter.setPen(Qt::NoPen);
    painter.setBrush(m_foreground);

    for (int y = 0; y < m_gridHeight; ++y)
    {
        for (int x = 0; x < m_gridWidth; ++x)
        {
            if (m_buffer[y * m_gridWidth + x])
                painter.drawRect(offsetX + x * scale, offsetY + y * scale, scale, scale);
        }
    }
}
