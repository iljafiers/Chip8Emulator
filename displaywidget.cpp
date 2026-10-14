#include "displaywidget.h"

#include <QPainter>
#include <algorithm>

const QList<DisplayThemeInfo> &displayThemes()
{
    static const QList<DisplayThemeInfo> themes = {
        {DisplayTheme::GreenOnBlack, "greenOnBlack", QT_TRANSLATE_NOOP("DisplayWidget", "Green on Black"),
         QColor(0, 220, 90), Qt::black, QColor(0, 110, 45), QColor(190, 255, 210)},
        {DisplayTheme::OrangeOnBlack, "orangeOnBlack", QT_TRANSLATE_NOOP("DisplayWidget", "Orange on Black"),
         QColor(255, 140, 0), Qt::black, QColor(140, 70, 0), QColor(255, 215, 140)},
        {DisplayTheme::WhiteOnBlack, "whiteOnBlack", QT_TRANSLATE_NOOP("DisplayWidget", "White on Black"),
         Qt::white, Qt::black, QColor(85, 85, 85), QColor(170, 170, 170)},
        {DisplayTheme::BlackOnWhite, "blackOnWhite", QT_TRANSLATE_NOOP("DisplayWidget", "Black on White"),
         Qt::black, Qt::white, QColor(170, 170, 170), QColor(85, 85, 85)},
        {DisplayTheme::Octo, "octo", QT_TRANSLATE_NOOP("DisplayWidget", "Octo"),
         QColor(0xFF, 0xCC, 0x00), QColor(0x99, 0x66, 0x00), QColor(0xFF, 0x66, 0x00), QColor(0x66, 0x22, 0x00)},
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
    m_image = QImage(Chip8::LORES_WIDTH, Chip8::LORES_HEIGHT, QImage::Format_Indexed8);
    m_image.setColorCount(4);
    m_image.fill(0);
    setMinimumSize(Chip8::LORES_WIDTH * 4, Chip8::LORES_HEIGHT * 4);
    setAutoFillBackground(true); // fills the margins around the image
    setTheme(DisplayTheme::OrangeOnBlack);
}

void DisplayWidget::setBuffer(const uint8_t *src, int width, int height)
{
    if (m_image.width() != width || m_image.height() != height)
    {
        QImage image(width, height, QImage::Format_Indexed8);
        image.setColorTable(m_image.colorTable());
        m_image = image;
    }

    // scanlines may be padded, so copy row by row
    for (int y = 0; y < height; ++y)
    {
        uchar         *line = m_image.scanLine(y);
        const uint8_t *row  = src + y * width;
        for (int x = 0; x < width; ++x)
            line[x] = row[x] & 3; // bit 0 is plane 1, bit 1 plane 2
    }
    update();
}

void DisplayWidget::setTheme(DisplayTheme theme)
{
    const DisplayThemeInfo &info = displayThemeInfo(theme);
    m_image.setColor(0, info.background.rgb());
    m_image.setColor(1, info.foreground.rgb());
    m_image.setColor(2, info.plane2.rgb());
    m_image.setColor(3, info.bothPlanes.rgb());

    // also used for the margins around the image
    QPalette pal = palette();
    pal.setColor(QPalette::Window, info.background);
    setPalette(pal);
    update();
}

int DisplayWidget::pixelScale() const
{
    const qreal dpr    = devicePixelRatioF();
    int         scaleX = static_cast<int>(width() * dpr) / m_image.width();
    int         scaleY = static_cast<int>(height() * dpr) / m_image.height();
    return std::max(1, std::min(scaleX, scaleY));
}

QSize DisplayWidget::sizeHint() const
{
    return QSize(Chip8::LORES_WIDTH * 10, Chip8::LORES_HEIGHT * 10);
}

void DisplayWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    // draw in device pixels, so pixel edges land on whole screen pixels
    const qreal dpr = devicePixelRatioF();
    painter.scale(1.0 / dpr, 1.0 / dpr);
    const int deviceWidth  = static_cast<int>(width() * dpr);
    const int deviceHeight = static_cast<int>(height() * dpr);

    int scale   = pixelScale();
    int offsetX = (deviceWidth - scale * m_image.width()) / 2;
    int offsetY = (deviceHeight - scale * m_image.height()) / 2;

    // nearest-neighbour scaling keeps the pixels sharp
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QRect(offsetX, offsetY, scale * m_image.width(), scale * m_image.height()), m_image);
}
