#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include "chip8.h"
#include "displaywidget.h"

class QLineEdit;
class QCheckBox;
class QComboBox;

class PreferencesDialog : public QDialog
{
    Q_OBJECT
public:
    // Speeds the quirk presets set: classic CHIP-8 and SUPER-CHIP games expect a
    // few hundred instructions per second, while XO-CHIP games are written for
    // Octo, which runs up to 1000 instructions per frame (60000 per second).
    static const int DEFAULT_IPS = 700;
    static const int XO_CHIP_IPS = 60000;

    explicit PreferencesDialog(QWidget *parent = nullptr);

    void setInstructionsPerSecond(int ips);
    int  instructionsPerSecond() const;

    void          setQuirks(const Chip8::Quirks &quirks);
    Chip8::Quirks quirks() const;

    void setSoundEnabled(bool enabled);
    bool soundEnabled() const;

    void         setTheme(DisplayTheme theme);
    DisplayTheme theme() const;

private:
    QLineEdit *m_edtIps;
    QCheckBox *m_chkSoundEnabled;
    QComboBox *m_cmbTheme;

    QCheckBox *m_chkShiftVxOnly;
    QCheckBox *m_chkLoadStoreIncrementsI;
    QCheckBox *m_chkJumpUsesVx;
    QCheckBox *m_chkClipSprites;
    QCheckBox *m_chkVfReset;
    QCheckBox *m_chkXoChip;
};

#endif // PREFERENCESDIALOG_H
