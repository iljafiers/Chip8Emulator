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
};

#endif // PREFERENCESDIALOG_H
