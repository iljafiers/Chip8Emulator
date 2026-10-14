#include "mainwindow.h"
#include "displaywidget.h"
#include "preferencesdialog.h"
#include "beeper.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QFile>
#include <QKeyEvent>
#include <QApplication>
#include <QIcon>
#include <QSettings>
#include <QtMath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // WM_ENTERSIZEMOVE / WM_EXITSIZEMOVE
#endif

namespace {
// Standard CHIP-8 keypad mapped onto a QWERTY keyboard:
//   1 2 3 4          1 2 3 C
//   Q W E R    ->    4 5 6 D
//   A S D F          7 8 9 E
//   Z X C V          A 0 B F

int mapQtKeyToChip8(int key)
{
    switch (key)
    {
        case Qt::Key_1: return 0x1;
        case Qt::Key_2: return 0x2;
        case Qt::Key_3: return 0x3;
        case Qt::Key_4: return 0xC;
        case Qt::Key_Q: return 0x4;
        case Qt::Key_W: return 0x5;
        case Qt::Key_E: return 0x6;
        case Qt::Key_R: return 0xD;
        case Qt::Key_A: return 0x7;
        case Qt::Key_S: return 0x8;
        case Qt::Key_D: return 0x9;
        case Qt::Key_F: return 0xE;
        case Qt::Key_Z: return 0xA;
        case Qt::Key_X: return 0x0;
        case Qt::Key_C: return 0xB;
        case Qt::Key_V: return 0xF;
        default: return -1;
    }
}

// Maps by physical key position, so the keypad works the same on any keyboard
// layout and isn't affected by Shift. Windows set-1 scan codes.
#ifdef Q_OS_WIN
int mapScanCodeToChip8(quint32 scanCode)
{
    switch (scanCode)
    {
        case 0x02: return 0x1; // 1
        case 0x03: return 0x2; // 2
        case 0x04: return 0x3; // 3
        case 0x05: return 0xC; // 4
        case 0x10: return 0x4; // Q
        case 0x11: return 0x5; // W
        case 0x12: return 0x6; // E
        case 0x13: return 0xD; // R
        case 0x1E: return 0x7; // A
        case 0x1F: return 0x8; // S
        case 0x20: return 0x9; // D
        case 0x21: return 0xE; // F
        case 0x2C: return 0xA; // Z
        case 0x2D: return 0x0; // X
        case 0x2E: return 0xB; // C
        case 0x2F: return 0xF; // V
        default: return -1;
    }
}

#endif

#ifdef Q_OS_MACOS
// macOS reports no scan code, but its virtual key codes (kVK_ANSI_* from
// HIToolbox/Events.h) also name physical key positions, independent of layout.
int mapMacVirtualKeyToChip8(quint32 virtualKey)
{
    switch (virtualKey)
    {
        case 0x12: return 0x1; // 1
        case 0x13: return 0x2; // 2
        case 0x14: return 0x3; // 3
        case 0x15: return 0xC; // 4
        case 0x0C: return 0x4; // Q
        case 0x0D: return 0x5; // W
        case 0x0E: return 0x6; // E
        case 0x0F: return 0xD; // R
        case 0x00: return 0x7; // A
        case 0x01: return 0x8; // S
        case 0x02: return 0x9; // D
        case 0x03: return 0xE; // F
        case 0x06: return 0xA; // Z
        case 0x07: return 0x0; // X
        case 0x08: return 0xB; // C
        case 0x09: return 0xF; // V
        default: return -1;
    }
}
#endif

int mapKeyEventToChip8(const QKeyEvent *event)
{
#if defined(Q_OS_WIN)
    if (event->nativeScanCode() != 0)
        return mapScanCodeToChip8(event->nativeScanCode());
#elif defined(Q_OS_MACOS)
    // kVK_ANSI_A is 0, so 0 can't mean "no native info"; only trust it for
    // events that came from the system rather than being synthesized
    if (event->spontaneous())
        return mapMacVirtualKeyToChip8(event->nativeVirtualKey());
#endif
    return mapQtKeyToChip8(event->key());
}

// Chip8 takes plain bytes, so it has no Qt dependency
bool loadROMData(Chip8 &chip8, const QByteArray &data)
{
    return chip8.loadROM(reinterpret_cast<const uint8_t *>(data.constData()),
                         static_cast<size_t>(data.size()));
}

// Toolbar icon with its own faded artwork for the disabled state, since Qt's
// automatic greying barely changes a mid-grey icon. Each state ships as a
// 24px and a 48px (@2x, for high-DPI screens) PNG.
QIcon toolbarIcon(const QString &name)
{
    QIcon icon;
    for (const QString &suffix : {QString(), QStringLiteral("@2x")})
    {
        icon.addFile(QString(":/icons/%1%2.png").arg(name, suffix), QSize(), QIcon::Normal);
        icon.addFile(QString(":/icons/%1-disabled%2.png").arg(name, suffix), QSize(), QIcon::Disabled);
    }
    return icon;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_displayWidget(new DisplayWidget(this))
    , m_beeper(new Beeper(this))
    , m_soundEnabled(false)
    , m_displayTheme(DisplayTheme::OrangeOnBlack)
    , m_cpuTimer(new QTimer(this))
    , m_fitTimer(new QTimer(this))
    , m_inSizeMove(false)
    , m_runState(RunState::Stopped)
    , m_instructionsPerSecond(700)
    , m_instructionAllotment(0)
    , m_stepTimerAllotment(0)
    , m_nextTickNs(0)
{
    setWindowTitle(tr("Chip8 Emulator"));
    setCentralWidget(m_displayWidget);

    loadSettings();
    m_chip8.setQuirks(m_quirks);
    m_displayWidget->setTheme(m_displayTheme);

    createActions();
    createMenus();
    createToolbar();
    createStatusBar();

    // also drives the 60Hz delay/sound timers, so it needs to be accurate
    m_cpuTimer->setTimerType(Qt::PreciseTimer);
    m_cpuTimer->setSingleShot(true); // rescheduled each tick, see scheduleNextTick()
    connect(m_cpuTimer, &QTimer::timeout, this, &MainWindow::onEmulationTick);

    // wait until a resize has settled before fitting the window to the display
    m_fitTimer->setSingleShot(true);
    m_fitTimer->setInterval(200);
    connect(m_fitTimer, &QTimer::timeout, this, &MainWindow::fitWindowToDisplay);

    updateRunningActions();
    resize(720, 460); // fitted to the display once the window is shown

    loadLastROM();
}

MainWindow::~MainWindow()
{
    // remember the ROM in memory, so the next start can load it again
    QSettings settings;
    if (m_romPath.isEmpty())
        settings.remove("rom/lastRom");
    else
        settings.setValue("rom/lastRom", m_romPath);
}

// Creates all actions with their shortcuts. Standard key sequences are used
// where Qt defines one for the platform; the run controls follow the usual
// debugger keys (F5 run, Shift+F5 stop, F10 step). None of these clash with
// the CHIP-8 keypad, which only uses unmodified letter and digit keys.
void MainWindow::createActions()
{
    m_actLoadROM = new QAction(tr("&Load ROM..."), this);
    m_actLoadROM->setShortcuts(QKeySequence::Open); // Ctrl+O
    connect(m_actLoadROM, &QAction::triggered, this, &MainWindow::onLoadROM);

    m_actExit = new QAction(tr("E&xit"), this);
    m_actExit->setShortcuts(QKeySequence::Quit);
    if (m_actExit->shortcuts().isEmpty()) // Windows has no standard key besides Alt+F4
        m_actExit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(m_actExit, &QAction::triggered, this, &MainWindow::onExit);

    m_actPreferences = new QAction(tr("&Preferences..."), this);
    m_actPreferences->setShortcuts(QKeySequence::Preferences);
    if (m_actPreferences->shortcuts().isEmpty())
        m_actPreferences->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(m_actPreferences, &QAction::triggered, this, &MainWindow::onPreferences);

    m_actAbout = new QAction(tr("&About"), this);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::onAbout);

    // Run controls. The toolbar and the Run menu share these actions, so they
    // are enabled and disabled together.
    m_actPlay = new QAction(toolbarIcon("play"), tr("&Play"), this);
    m_actPlay->setShortcut(QKeySequence(Qt::Key_F5));
    connect(m_actPlay, &QAction::triggered, this, &MainWindow::onPlay);

    m_actPause = new QAction(toolbarIcon("pause"), tr("Pause"), this);
    connect(m_actPause, &QAction::triggered, this, &MainWindow::onPause);

    m_actStop = new QAction(toolbarIcon("stop"), tr("&Stop"), this);
    m_actStop->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
    connect(m_actStop, &QAction::triggered, this, &MainWindow::onStop);

    m_actStep = new QAction(toolbarIcon("step"), tr("S&tep"), this);
    m_actStep->setShortcut(QKeySequence(Qt::Key_F10));
    connect(m_actStep, &QAction::triggered, this, &MainWindow::onStep);

    // Run > Pause: ticked while paused. Toggles between paused and running.
    m_actTogglePause = new QAction(tr("P&ause"), this);
    m_actTogglePause->setCheckable(true);
    // Space as well, since many laptops have no Pause key
    m_actTogglePause->setShortcuts({QKeySequence(Qt::Key_Pause), QKeySequence(Qt::Key_Space)});
    connect(m_actTogglePause, &QAction::triggered, this, &MainWindow::onTogglePause);

    // toolbar tooltips show the shortcut, as the buttons have no menu text
    for (QAction *action : {m_actPlay, m_actStop, m_actStep})
        action->setToolTip(QString("%1 (%2)").arg(action->iconText(),
                                                  action->shortcut().toString(QKeySequence::NativeText)));
    QStringList pauseKeys;
    for (const QKeySequence &key : m_actTogglePause->shortcuts())
        pauseKeys << key.toString(QKeySequence::NativeText);
    m_actPause->setToolTip(QString("%1 (%2)").arg(m_actPause->iconText(), pauseKeys.join(" / ")));
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_actLoadROM);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actExit);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_actPreferences);

    QMenu *runMenu = menuBar()->addMenu(tr("&Run"));
    runMenu->addAction(m_actPlay);
    runMenu->addAction(m_actTogglePause);
    runMenu->addAction(m_actStop);
    runMenu->addSeparator();
    runMenu->addAction(m_actStep);

    // on macOS, Qt moves About into the application menu
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_actAbout);
}

void MainWindow::createToolbar()
{
    QToolBar *toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setMovable(false);

    toolbar->addAction(m_actPause);
    toolbar->addAction(m_actPlay);
    toolbar->addAction(m_actStop);
    toolbar->addAction(m_actStep);
}

void MainWindow::createStatusBar()
{
    m_statusROMLabel        = new QLabel(tr("No ROM loaded"), this);
    m_statusStateLabel      = new QLabel(tr("Stopped"), this);
    m_statusResolutionLabel = new QLabel(this);
    m_statusIpsLabel        = new QLabel(tr("%1 IPS").arg(m_instructionsPerSecond), this);
    updateResolutionLabel();

    statusBar()->addWidget(m_statusROMLabel, 1);
    statusBar()->addPermanentWidget(m_statusStateLabel);
    statusBar()->addPermanentWidget(m_statusResolutionLabel);
    statusBar()->addPermanentWidget(m_statusIpsLabel);
}

// settings
void MainWindow::loadSettings()
{
    QSettings           settings;
    const Chip8::Quirks defaults;

    m_instructionsPerSecond = settings.value("speed/instructionsPerSecond", m_instructionsPerSecond).toInt();
    if (m_instructionsPerSecond <= 0)
        m_instructionsPerSecond = 700;

    m_soundEnabled = settings.value("sound/enabled", m_soundEnabled).toBool();

    m_displayTheme = displayThemeFromKey(settings.value("display/theme").toString(), m_displayTheme);

    settings.beginGroup("quirks");
    m_quirks.shiftUsesVxOnly      = settings.value("shiftUsesVxOnly", defaults.shiftUsesVxOnly).toBool();
    m_quirks.loadStoreIncrementsI = settings.value("loadStoreIncrementsI", defaults.loadStoreIncrementsI).toBool();
    m_quirks.jumpUsesVx           = settings.value("jumpUsesVx", defaults.jumpUsesVx).toBool();
    m_quirks.clipSprites          = settings.value("clipSprites", defaults.clipSprites).toBool();
    m_quirks.vfReset              = settings.value("vfReset", defaults.vfReset).toBool();
    settings.endGroup();
}

void MainWindow::saveSettings() const
{
    QSettings settings;

    settings.setValue("speed/instructionsPerSecond", m_instructionsPerSecond);
    settings.setValue("sound/enabled", m_soundEnabled);
    settings.setValue("display/theme", QString::fromLatin1(displayThemeInfo(m_displayTheme).key));

    settings.beginGroup("quirks");
    settings.setValue("shiftUsesVxOnly", m_quirks.shiftUsesVxOnly);
    settings.setValue("loadStoreIncrementsI", m_quirks.loadStoreIncrementsI);
    settings.setValue("jumpUsesVx", m_quirks.jumpUsesVx);
    settings.setValue("clipSprites", m_quirks.clipSprites);
    settings.setValue("vfReset", m_quirks.vfReset);
    settings.endGroup();
}

// timer
void MainWindow::startTimer()
{
    m_tickClock.start();
    m_nextTickNs = 0;
    scheduleNextTick();
    m_instructionAllotment = 0;
}

// QTimer only takes whole milliseconds and a repeating 16 ms timer runs at
// 62.5Hz, so each tick is scheduled against a fixed 1/60 s timeline instead.
// Rounding and lateness then even out, giving exactly 60Hz on average.
void MainWindow::scheduleNextTick()
{
    const qint64 now = m_tickClock.nsecsElapsed();
    m_nextTickNs += 1000000000LL / TICKS_PER_SECOND;

    // after a long stall (e.g. a blocked event loop) don't race to catch up
    if (m_nextTickNs < now - 100000000LL)
        m_nextTickNs = now;

    const qint64 delayMs = (m_nextTickNs - now + 500000) / 1000000;
    m_cpuTimer->start(static_cast<int>(qMax<qint64>(0, delayMs)));
}

void MainWindow::stopTimer()
{
    m_cpuTimer->stop();
    m_instructionAllotment = 0;
}

void MainWindow::onLoadROM()
{
    // start in the folder the last ROM was loaded from
    QSettings settings;
    QString   path = QFileDialog::getOpenFileName(
        this, tr("Load ROM"), settings.value("rom/lastFolder").toString(),
        tr("Chip8 ROMs (*.ch8 *.c8 *.rom *.*)"));
    if (path.isEmpty())
        return;

    if (loadROMFile(path, true))
        settings.setValue("rom/lastFolder", QFileInfo(path).absolutePath());
}

// Reloads the ROM that was in memory when the app was last closed, without
// starting it. Any failure (no ROM stored, file moved, too large) is silent
// and leaves the emulator empty.
void MainWindow::loadLastROM()
{
    QString path = QSettings().value("rom/lastRom").toString();
    if (!path.isEmpty())
        loadROMFile(path, false);
}

// Loads a ROM file and leaves the emulator paused on its first instruction.
// Returns false, keeping the current ROM, if the file can't be used; with
// showErrors set, the reason is shown in a message box.
bool MainWindow::loadROMFile(const QString &path, bool showErrors)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (showErrors)
            QMessageBox::warning(this, tr("Load ROM"),
                                 tr("Could not open file:\n%1").arg(path));
        return false;
    }

    QByteArray data = file.readAll();
    m_chip8.setQuirks(m_quirks);
    if (!loadROMData(m_chip8, data))
    {
        if (showErrors)
            QMessageBox::warning(this, tr("Load ROM"),
                                 tr("ROM is too large to fit in memory."));
        return false;
    }

    m_romPath = QFileInfo(path).absoluteFilePath();
    m_romData = data;
    m_statusROMLabel->setText(tr("Loaded: %1").arg(QFileInfo(path).fileName()));
    // loading resets to low resolution, which matters if the previous ROM was in hires
    showDisplay(m_displayWidget->gridWidth() != m_chip8.displayWidth());

    onPause();
    return true;
}

void MainWindow::onExit()
{
    QApplication::quit();
}

void MainWindow::onPreferences()
{
    PreferencesDialog dlg(this);
    dlg.setInstructionsPerSecond(m_instructionsPerSecond);
    dlg.setQuirks(m_quirks);
    dlg.setSoundEnabled(m_soundEnabled);
    dlg.setTheme(m_displayTheme);

    if (dlg.exec() == QDialog::Accepted)
    {
        m_instructionsPerSecond = dlg.instructionsPerSecond();
        m_soundEnabled          = dlg.soundEnabled();
        if (!m_soundEnabled)
            m_beeper->setActive(false);
        m_displayTheme = dlg.theme();
        m_displayWidget->setTheme(m_displayTheme);
        m_quirks = dlg.quirks();
        m_chip8.setQuirks(m_quirks);
        m_statusIpsLabel->setText(tr("%1 IPS").arg(m_instructionsPerSecond));
        saveSettings();
    }
}

// Run > Pause and the Pause key: resume when paused, otherwise pause (from
// Stopped too, like the toolbar's Pause button, ready for stepping).
void MainWindow::onTogglePause()
{
    if (m_runState == RunState::Paused)
        onPlay();
    else if (m_runState != RunState::Error)
        onPause();
    else
        updateRunningActions(); // undo the automatic check toggle
}

void MainWindow::onAbout()
{
    // CHIP-8 keypad layout, with the PC key that maps to each position
    static const char *chipKeys[4][4] = {
        {"1", "2", "3", "C"},
        {"4", "5", "6", "D"},
        {"7", "8", "9", "E"},
        {"A", "0", "B", "F"}};
    static const char *pcKeys[4][4] = {
        {"1", "2", "3", "4"},
        {"Q", "W", "E", "R"},
        {"A", "S", "D", "F"},
        {"Z", "X", "C", "V"}};

    QString keypad = "<table border=\"1\" cellspacing=\"0\" cellpadding=\"6\" align=\"center\">";
    for (int row = 0; row < 4; ++row)
    {
        keypad += "<tr>";
        for (int col = 0; col < 4; ++col)
        {
            keypad += QString("<td align=\"center\" width=\"36\">"
                              "<span style=\"font-size:14pt; font-weight:bold;\">%1</span><br>"
                              "<span style=\"font-size:8pt; color:gray;\">%2</span></td>")
                          .arg(chipKeys[row][col], pcKeys[row][col]);
        }
        keypad += "</tr>";
    }
    keypad += "</table>";

    QMessageBox::about(this, tr("About Chip8 Emulator"),
                       tr("<b>Chip8 Emulator</b> %1<br>"
                          "A small CHIP-8 interpreter with a Qt Widgets front-end.<br><br>")
                               .arg(QApplication::applicationVersion())
                           + tr("Keypad: 1234 / QWER / ASDF / ZXCV maps to the CHIP-8 keys 1-9, A-F.")
                           + "<br><br>" + keypad
                           + tr("<p align=\"center\"><small>CHIP-8 key, with the PC key below it</small></p>"));
}

void MainWindow::onPlay()
{
    if (m_romData.isEmpty())
        return; // nothing to run

    // after 00FD there is nothing left to run, so Play starts the program again
    if (m_chip8.hasExited())
        onStop();

    if (!m_chip8.hasError())
    {
        m_runState = RunState::Running;
        startTimer();
    }
    updateRunningActions();
}

void MainWindow::onPause()
{
    m_runState = RunState::Paused;
    m_cpuTimer->stop();

    updateRunningActions();
}

void MainWindow::onStop()
{
    m_runState = RunState::Stopped;
    stopTimer();
    m_stepTimerAllotment = 0;

    m_chip8.setQuirks(m_quirks);
    if (!m_romData.isEmpty())
        loadROMData(m_chip8, m_romData); // resets the machine and reloads the ROM
    else
        m_chip8.reset(true);
    showDisplay(m_displayWidget->gridWidth() != m_chip8.displayWidth());

    updateRunningActions();
}

void MainWindow::onStep()
{
    if (m_romData.isEmpty() || m_chip8.hasError() || m_chip8.hasExited())
        return;

    // tick the delay/sound timers as often as they would tick while running,
    // otherwise a program waiting on the delay timer never gets past it
    m_stepTimerAllotment += TICKS_PER_SECOND;
    if (m_stepTimerAllotment >= m_instructionsPerSecond)
    {
        m_stepTimerAllotment -= m_instructionsPerSecond;
        m_chip8.tickTimers();
    }

    Chip8::StepResult result = m_chip8.step();
    handleStepResult(result);
    if (result == Chip8::StepResult::Exited)
    {
        onProgramExited();
        return;
    }

    if (m_chip8.hasError())
        m_runState = RunState::Error;

    updateRunningActions();

    if (m_runState == RunState::Error)
        m_statusStateLabel->setText(tr("Error: %1").arg(QString::fromStdString(m_chip8.errorString())));
}

void MainWindow::onEmulationTick()
{
    scheduleNextTick();

    if (m_runState == RunState::Running)
    {
        m_instructionAllotment += m_instructionsPerSecond;

        m_chip8.tickTimers();

        while (m_instructionAllotment >= TICKS_PER_SECOND)
        {
            m_instructionAllotment -= TICKS_PER_SECOND;

            // do one instruction
            Chip8::StepResult result = m_chip8.step();
            handleStepResult(result);
            if (result == Chip8::StepResult::Exited)
            {
                onProgramExited();
                break;
            }
            if (m_chip8.hasError())
            {
                m_runState = RunState::Error;
                m_cpuTimer->stop();
                m_beeper->setActive(false); // don't keep beeping while the message is open
                updateRunningActions();
                m_statusStateLabel->setText(tr("Error: %1").arg(QString::fromStdString(m_chip8.errorString())));
                showErrorMessage();
                break;
            }
        }

        m_beeper->setActive(m_soundEnabled && m_runState == RunState::Running && m_chip8.soundTimer() > 0);
    }
}

// Updates the screen after an instruction, switching resolution if it asked for that.
void MainWindow::handleStepResult(Chip8::StepResult result)
{
    switch (result)
    {
        case Chip8::StepResult::Ok:
        case Chip8::StepResult::Exited: // the last frame stays as it is
            break;
        case Chip8::StepResult::GraphicsChanged:
            showDisplay(false);
            break;
        case Chip8::StepResult::ModeToHigh:
        case Chip8::StepResult::ModeToLow:
            showDisplay(true);
            break;
    }
}

// The program ended itself with 00FD: stop running but keep the last frame on
// screen. Play restarts the program from the beginning.
void MainWindow::onProgramExited()
{
    m_runState = RunState::Stopped;
    stopTimer();
    updateRunningActions();
    m_statusStateLabel->setText(tr("Program exited"));
}

// Copies the CHIP-8 frame to the display widget at the current resolution.
// Both modes are 2:1, so the widget just rescales the image to fit the same
// window; after a mode switch only the resolution in the status bar changes.
void MainWindow::showDisplay(bool modeChanged)
{
    m_displayWidget->setBuffer(m_chip8.displayBuffer(), m_chip8.displayWidth(), m_chip8.displayHeight());
    if (modeChanged)
        updateResolutionLabel();
}

void MainWindow::updateResolutionLabel()
{
    m_statusResolutionLabel->setText(QString("%1x%2").arg(m_chip8.displayWidth()).arg(m_chip8.displayHeight()));
}

// Popup with the CPU state at the point the emulator stopped on an error.
void MainWindow::showErrorMessage()
{
    QString type;
    switch (m_chip8.error())
    {
        case Chip8::Error::InvalidOpcode: type = tr("Invalid opcode"); break;
        case Chip8::Error::StackUnderflow: type = tr("Return with an empty stack"); break;
        case Chip8::Error::PCOutOfBounds: type = tr("Program counter out of bounds"); break;
        default: type = tr("Unknown error"); break;
    }

    auto hex = [](unsigned value) { return "0x" + QString("%1").arg(value, 4, 16, QChar('0')).toUpper(); };

    // with the PC out of bounds there was no instruction to fetch
    QString opcode = m_chip8.error() == Chip8::Error::PCOutOfBounds
                         ? tr("n/a")
                         : hex(m_chip8.errorOpcode());

    QString details = QString("PC:          %1\n"
                              "Instruction: %2\n"
                              "SP:          %3\n"
                              "I:           %4\n\n")
                          .arg(hex(m_chip8.errorPC()), opcode)
                          .arg(m_chip8.SP())
                          .arg(m_chip8.I());

    // 4x4 grid filled column by column: V0-V3 down the first column, V4-V7 the next, ...
    for (unsigned row = 0; row < 4; ++row)
    {
        for (unsigned col = 0; col < 4; ++col)
        {
            unsigned x = col * 4 + row;
            details += QString("V%1: %2").arg(x, 0, 16).toUpper().arg(m_chip8.V(x), 3);
            details += (col == 3) ? "\n" : "    ";
        }
    }

    QMessageBox box(QMessageBox::Critical, tr("Emulation error"), type, QMessageBox::Ok, this);
    box.setInformativeText("<pre>" + details.toHtmlEscaped() + "</pre>");
    box.exec();
}

void MainWindow::updateRunningActions()
{
    if (m_runState != RunState::Running)
        m_beeper->setActive(false);

    m_actTogglePause->blockSignals(true);
    m_actTogglePause->setChecked(m_runState == RunState::Paused); // ticked while paused
    m_actTogglePause->blockSignals(false);

    QString statusLabelText;
    switch (m_runState)
    {
        case RunState::Stopped:
            statusLabelText = tr("Stopped");
            break;
        case RunState::Running:
            statusLabelText = tr("Running");
            break;
        case RunState::Paused:
        {
            QString v;
            for (int x = 0; x < 16; x++)
            {
                v += QString("%1 ").arg(m_chip8.V(x), 0, 10);
            }
            statusLabelText = QString("Paused, PC=0x%1, I=%2, V=%3").arg(m_chip8.PC(), 3, 16).arg(m_chip8.I(), 3, 16).arg(v);
        }
        break;
        case RunState::Error:
            statusLabelText = tr("Error");
            break;
    }
    m_statusStateLabel->setText(statusLabelText);

    // without a ROM there is nothing to run, step through or reset
    const bool hasROM = !m_romData.isEmpty();
    m_actTogglePause->setEnabled(hasROM && m_runState != RunState::Error);
    m_actPlay->setEnabled(hasROM && (m_runState == RunState::Stopped || m_runState == RunState::Paused));
    m_actPause->setEnabled(hasROM && (m_runState == RunState::Running || m_runState == RunState::Stopped));
    m_actStop->setEnabled(hasROM);
    m_actStep->setEnabled(hasROM && m_runState == RunState::Paused);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    int chipKey = mapKeyEventToChip8(event);
    if (chipKey < 0)
    {
        QMainWindow::keyPressEvent(event);
        return;
    }

    // auto-repeat would look like extra presses to FX0A
    if (!event->isAutoRepeat())
    {
        // remember which CHIP-8 key this physical key pressed, so the release
        // maps to the same key even if a modifier changed event->key() meanwhile.
        // macOS reports no scan code, but maps by virtual key, which is just as stable.
        if (event->nativeScanCode() != 0)
            m_pressedKeys.insert(event->nativeScanCode(), chipKey);
        m_chip8.setKeyState(chipKey, true);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    const quint32 scanCode = event->nativeScanCode();
    int           chipKey  = (scanCode != 0 && m_pressedKeys.contains(scanCode)) ? m_pressedKeys.take(scanCode)
                                                                                 : mapKeyEventToChip8(event);

    if (chipKey >= 0)
        m_chip8.setKeyState(chipKey, false);
    else
        QMainWindow::keyReleaseEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    m_fitTimer->start(); // restarts while the size keeps changing
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    // While the user drags the frame, resizing the window would fight the mouse,
    // so wait for the drag to end and fit the window then.
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->message == WM_ENTERSIZEMOVE)
        m_inSizeMove = true;
    else if (msg->message == WM_EXITSIZEMOVE)
    {
        m_inSizeMove = false;
        fitWindowToDisplay();
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

// Shrinks the window so the display area is an exact multiple of 128x64. That
// leaves no margin around the image in either resolution: low resolution is
// drawn at exactly twice the hires scale. Maximized and full-screen windows
// are left alone.
void MainWindow::fitWindowToDisplay()
{
    if (m_inSizeMove || isMaximized() || isFullScreen() || !isVisible())
        return;

    // work in device pixels, so this also fits with fractional display scaling
    const qreal dpr     = m_displayWidget->devicePixelRatioF();
    const QSize display = m_displayWidget->size();
    const int   scale   = qMax(1, qMin(static_cast<int>(display.width() * dpr) / Chip8::HIRES_WIDTH,
                                       static_cast<int>(display.height() * dpr) / Chip8::HIRES_HEIGHT));
    // back to logical pixels; rounding up can leave at most one device pixel spare
    const QSize fitted(qCeil(Chip8::HIRES_WIDTH * scale / dpr - 0.001),
                       qCeil(Chip8::HIRES_HEIGHT * scale / dpr - 0.001));

    // the resize event this causes finds the display already fitted
    if (fitted != display)
        resize(size() - display + fitted);
}

void MainWindow::changeEvent(QEvent *event)
{
    // the release of a key held while switching away never arrives here,
    // so let go of everything when the window is deactivated
    if (event->type() == QEvent::ActivationChange && !isActiveWindow())
        releaseAllKeys();

    QMainWindow::changeEvent(event);
}

void MainWindow::releaseAllKeys()
{
    for (int key = 0; key < 16; ++key)
        m_chip8.setKeyState(key, false);
    m_pressedKeys.clear();
}
