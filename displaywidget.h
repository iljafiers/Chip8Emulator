#ifndef DISPLAYWIDGET_H
#define DISPLAYWIDGET_H

#include <QWidget>
#include <QColor>
#include <QImage>
#include "chip8.h"

// Colour schemes for the CHIP-8 screen: pixel colour on background colour.
enum class DisplayTheme
{
    GreenOnBlack,
    OrangeOnBlack, // like the amber/orange monochrome monitors of old
    WhiteOnBlack,
    BlackOnWhite
};

struct DisplayThemeInfo
{
    DisplayTheme theme;
    const char  *key;  // stored in the settings
    const char  *name; // shown in the preferences dialog
    QColor       foreground;
    QColor       background;
};

// All themes, in the order they are listed in the preferences dialog.
const QList<DisplayThemeInfo> &displayThemes();
const DisplayThemeInfo        &displayThemeInfo(DisplayTheme theme);
// Theme with the given settings key, or the fallback if there is none.
DisplayTheme displayThemeFromKey(const QString &key, DisplayTheme fallback);

class DisplayWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DisplayWidget(QWidget *parent = nullptr);

    // Copies a width x height frame (64x32 or 128x64), one byte per pixel.
    void setBuffer(const uint8_t *buffer, int width, int height);
    int  gridWidth() const { return m_image.width(); } // resolution of the current frame
    int  gridHeight() const { return m_image.height(); }

    void setTheme(DisplayTheme theme);

    // Physical screen pixels per CHIP-8 pixel at the current widget size. This is
    // in device pixels, not Qt's logical pixels, so every CHIP-8 pixel is the
    // same size even with fractional display scaling (e.g. 125% or 150%).
    int pixelScale() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // The CHIP-8 frame at its native resolution, one byte per pixel. It is indexed:
    // colour 0 is the theme background and colour 1 the foreground, so changing
    // the theme only swaps the colour table.
    QImage m_image;
};

#endif // DISPLAYWIDGET_H
