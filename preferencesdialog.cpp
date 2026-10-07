#include "preferencesdialog.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QIntValidator>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCoreApplication>

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));

    // --- Speed setting -------------------------------------------------
    m_edtIps = new QLineEdit(this);
    m_edtIps->setValidator(new QIntValidator(1, 1000000, this));
    m_edtIps->setText("700");

    QFormLayout *speedLayout = new QFormLayout;
    speedLayout->addRow(tr("Instructions per second:"), m_edtIps);

    // --- Sound -----------------------------------------------------------
    m_chkSoundEnabled = new QCheckBox(tr("Sound enabled"), this);
    m_chkSoundEnabled->setChecked(false);
    speedLayout->addRow(m_chkSoundEnabled);

    // --- Display theme ---------------------------------------------------
    m_cmbTheme = new QComboBox(this);
    for (const DisplayThemeInfo &info : displayThemes())
        m_cmbTheme->addItem(QCoreApplication::translate("DisplayWidget", info.name), static_cast<int>(info.theme));
    setTheme(DisplayTheme::OrangeOnBlack);
    speedLayout->addRow(tr("Display theme:"), m_cmbTheme);

    // --- Quirks ----------------------------------------------------------
    QGroupBox *quirksBox = new QGroupBox(tr("Quirks"), this);

    m_chkShiftVxOnly = new QCheckBox(
        tr("8XY6/8XYE shift Vx directly (ignore Vy)"), quirksBox);
    m_chkLoadStoreIncrementsI = new QCheckBox(
        tr("FX55/FX65 increment I (COSMAC VIP behavior)"), quirksBox);
    m_chkJumpUsesVx = new QCheckBox(
        tr("BNNN jumps to Vx + NNN instead of V0 + NNN"), quirksBox);
    m_chkClipSprites = new QCheckBox(
        tr("Clip sprites at screen edge instead of wrapping"), quirksBox);
    m_chkVfReset = new QCheckBox(
        tr("8XY1/8XY2/8XY3 reset VF to 0 (COSMAC VIP behavior)"), quirksBox);

    QVBoxLayout *quirksLayout = new QVBoxLayout;
    quirksLayout->addWidget(m_chkShiftVxOnly);
    quirksLayout->addWidget(m_chkLoadStoreIncrementsI);
    quirksLayout->addWidget(m_chkJumpUsesVx);
    quirksLayout->addWidget(m_chkClipSprites);
    quirksLayout->addWidget(m_chkVfReset);

    // presets set all checkboxes at once
    QPushButton *btnPresetVip   = new QPushButton(tr("CHIP-8 (COSMAC VIP)"), quirksBox);
    QPushButton *btnPresetSchip = new QPushButton(tr("CHIP-48 / SUPER-CHIP"), quirksBox);
    btnPresetVip->setAutoDefault(false);
    btnPresetSchip->setAutoDefault(false);
    btnPresetVip->setToolTip(tr("Original 1977 interpreter; most early games such as Animal Race"));
    btnPresetSchip->setToolTip(tr("HP48 interpreters; most 1990s games such as Space Invaders"));
    connect(btnPresetVip, &QPushButton::clicked, this, [this]() {
        setQuirks(Chip8::Quirks::cosmacVip());
    });
    connect(btnPresetSchip, &QPushButton::clicked, this, [this]() {
        setQuirks(Chip8::Quirks::superChip());
    });

    QHBoxLayout *presetLayout = new QHBoxLayout;
    presetLayout->addWidget(new QLabel(tr("Presets:"), quirksBox));
    presetLayout->addWidget(btnPresetVip);
    presetLayout->addWidget(btnPresetSchip);
    presetLayout->addStretch();
    quirksLayout->addLayout(presetLayout);
    quirksBox->setLayout(quirksLayout);

    // --- Buttons ---------------------------------------------------------
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(speedLayout);
    mainLayout->addWidget(quirksBox);
    mainLayout->addWidget(buttons);
}

void PreferencesDialog::setInstructionsPerSecond(int ips)
{
    m_edtIps->setText(QString::number(ips));
}

int PreferencesDialog::instructionsPerSecond() const
{
    bool ok    = false;
    int  value = m_edtIps->text().toInt(&ok);
    if (!ok || value <= 0)
        return 700;
    return value;
}

void PreferencesDialog::setSoundEnabled(bool enabled)
{
    m_chkSoundEnabled->setChecked(enabled);
}

bool PreferencesDialog::soundEnabled() const
{
    return m_chkSoundEnabled->isChecked();
}

void PreferencesDialog::setTheme(DisplayTheme theme)
{
    int index = m_cmbTheme->findData(static_cast<int>(theme));
    if (index >= 0)
        m_cmbTheme->setCurrentIndex(index);
}

DisplayTheme PreferencesDialog::theme() const
{
    return static_cast<DisplayTheme>(m_cmbTheme->currentData().toInt());
}

void PreferencesDialog::setQuirks(const Chip8::Quirks &quirks)
{
    m_chkShiftVxOnly->setChecked(quirks.shiftUsesVxOnly);
    m_chkLoadStoreIncrementsI->setChecked(quirks.loadStoreIncrementsI);
    m_chkJumpUsesVx->setChecked(quirks.jumpUsesVx);
    m_chkClipSprites->setChecked(quirks.clipSprites);
    m_chkVfReset->setChecked(quirks.vfReset);
}

Chip8::Quirks PreferencesDialog::quirks() const
{
    Chip8::Quirks q;
    q.shiftUsesVxOnly      = m_chkShiftVxOnly->isChecked();
    q.loadStoreIncrementsI = m_chkLoadStoreIncrementsI->isChecked();
    q.jumpUsesVx           = m_chkJumpUsesVx->isChecked();
    q.clipSprites          = m_chkClipSprites->isChecked();
    q.vfReset              = m_chkVfReset->isChecked();
    return q;
}
