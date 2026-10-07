#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QByteArray>
#include <QString>
#include <QLabel>
#include <QHash>

#include "chip8.h"
#include "displaywidget.h"

class Beeper;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    // Menu actions
    void onLoadROM();
    void onExit();
    void onPreferences();
    void onTogglePause();
    void onAbout();

    // Toolbar actions
    void onPlay();
    void onPause();
    void onStop();
    void onStep();

    void onEmulationTick();

private:
    void createActions();
    void createMenus();
    void createToolbar();
    void createStatusBar();
    void updateRunningActions();
    void releaseAllKeys();
    // ROM loading
    bool loadROMFile(const QString &path, bool showErrors);
    void loadLastROM();
    // settings
    void loadSettings();
    void saveSettings() const;
    // timer
    void startTimer();
    void stopTimer();
    void scheduleNextTick();
    void showErrorMessage();
    // display
    void handleStepResult(Chip8::StepResult result);
    void onProgramExited();
    void showDisplay(bool modeChanged);
    void updateResolutionLabel();
    void fitWindowToDisplay();

private:
    enum class RunState
    {
        Stopped,
        Running,
        Paused,
        Error
    };

    Chip8         m_chip8;
    Chip8::Quirks m_quirks;

    DisplayWidget *m_displayWidget;
    Beeper        *m_beeper; // sounds while the CHIP-8 sound timer is non-zero
    bool           m_soundEnabled;
    DisplayTheme   m_displayTheme;
    QTimer        *m_cpuTimer;   // 60Hz tick: runs instructions and the delay/sound timers
    QTimer        *m_fitTimer;   // fits the window to the display shortly after a resize
    bool           m_inSizeMove; // Windows: the user is dragging the window frame

    RunState                  m_runState;
    int                       m_instructionsPerSecond;
    int                       m_instructionAllotment; // keeps track of instructions to run
    int                       m_stepTimerAllotment;   // Step: ticks the 60Hz timers at the configured speed
    QElapsedTimer             m_tickClock;            // time base for scheduling the 60Hz ticks
    qint64                    m_nextTickNs;           // when the next tick is due, on m_tickClock
    static constexpr uint16_t TICKS_PER_SECOND = 60;

    QString    m_romPath;
    QByteArray m_romData;

    QHash<quint32, int> m_pressedKeys; // native scan code -> CHIP-8 key it pressed

    // Actions. Play, Pause, Stop and Step appear in both the toolbar and the
    // Run menu; Run > Pause itself is the checkable m_actTogglePause.
    QAction *m_actLoadROM;
    QAction *m_actExit;
    QAction *m_actPreferences;
    QAction *m_actAbout;
    QAction *m_actPlay;
    QAction *m_actPause;
    QAction *m_actTogglePause;
    QAction *m_actStop;
    QAction *m_actStep;

    // Status bar widgets
    QLabel *m_statusROMLabel;
    QLabel *m_statusStateLabel;
    QLabel *m_statusResolutionLabel; // 64x32 or 128x64
    QLabel *m_statusIpsLabel;
};

#endif // MAINWINDOW_H
