# Chip8 Emulator

A CHIP-8 interpreter with a Qt 6 Widgets front-end. It runs classic 64×32 CHIP-8 programs and SUPER-CHIP programs, including the 128×64 high-resolution mode. It also has selectable compatibility quirks, sound, and a step-by-step debug mode.

## Building

Requirements:
- Qt 6 with the **widgets** and **multimedia** modules. It has been built with 6.7.1 and 6.10.1, using MinGW and MSVC.
- A C++17 compiler.

Open `Chip8Emulator.pro` in Qt Creator, or build from the command line:

```
qmake Chip8Emulator.pro
make            # or mingw32-make / nmake, depending on the toolchain
```

The toolbar icons are PNGs compiled into the executable (`resources.qrc`), so the Qt SVG module is not needed.

The version number is set once, as `VERSION` in `Chip8Emulator.pro`. It appears in the About box and, on Windows, in the `.exe` file properties. The application icon is one of the invaders from David Winter's Space Invaders ROM (the sprite at `0x3D3`), in orange on black. `icons/app.ico` holds it at 16–256 px for the `.exe`, and `icons/app.png` is used as the window icon.

### Code style

The code style is defined in `.clang-format`: 4 spaces (no tabs), braces on their own line, and your own line breaks are kept. To format all sources, run:

```
clang-format -i *.cpp *.h
```

Qt Creator ships `clang-format` in `Tools/QtCreator/bin/clang/bin` (under your Qt folder), and its ClangFormat settings can apply the style while you type.

## Using it

1. **File → Load ROM...** loads a `.ch8` file. The emulator starts out paused.
   - The dialog opens in the folder the last ROM was loaded from.
   - The ROM that is in memory when the app closes is loaded again at the next start, paused and not yet running.
   - If that ROM can't be loaded (moved, deleted or too large), the app starts empty, without an error message.
2. Use **Play / Pause / Stop / Step** on the toolbar or in the **Run** menu. These are disabled until a ROM is loaded. **Run → Pause** is ticked while the emulator is paused.
   - **Stop** resets the machine and reloads the current ROM.
   - **Step** runs one instruction. It is only available while paused. The delay and sound timers count down as often as they would while running at the configured speed.
   - While paused, the status bar shows PC, I and V0–VF.
3. **Edit → Preferences...** sets the speed (instructions per second), sound, the display theme, and the compatibility quirks. Settings are saved between sessions using `QSettings`.

### Shortcuts

| Action | Shortcut |
|---|---|
| File → Load ROM... | Ctrl+O |
| File → Exit | Ctrl+Q (and Alt+F4) |
| Edit → Preferences... | Ctrl+, |
| Run → Play | F5 |
| Run → Pause (toggle) | Pause/Break or Space |
| Run → Stop | Shift+F5 |
| Run → Step | F10 |

The run keys follow the usual debugger convention. Load ROM uses the platform's standard Open key, and Exit and Preferences use the platform standard where there is one (for example Cmd+Q and Cmd+, on macOS). None of the shortcuts clash with the CHIP-8 keypad.

### Keypad

The 16-key CHIP-8 keypad is mapped onto the left side of the keyboard:

```
PC keyboard        CHIP-8 keypad
1 2 3 4            1 2 3 C
Q W E R    ->      4 5 6 D
A S D F            7 8 9 E
Z X C V            A 0 B F
```

On Windows, keys are matched by physical position (scan code), so the layout works on AZERTY, QWERTZ and similar keyboards, and isn't affected by Shift. **About** shows the mapping as a table.

## Code overview

| File | Contents |
|---|---|
| `chip8.h/.cpp` | `Chip8`: the interpreter core, in plain C++17 with no Qt dependency, so it can be reused outside this GUI. Random numbers come from `rand()`, seeded once from the clock. |
| `mainwindow.h/.cpp` | `MainWindow`: menus, toolbar, status bar, keyboard input, timing, settings, error reporting |
| `displaywidget.h/.cpp` | `DisplayWidget`: draws the frame buffer (64×32 or 128×64), scaled and centred, in the selected colour theme |
| `beeper.h/.cpp` | `Beeper` and `SquareWaveGenerator`: the 440 Hz buzzer tone |
| `preferencesdialog.h/.cpp` | `PreferencesDialog`: speed, sound, display theme and quirk settings |
| `main.cpp` | Creates the application and the main window |
| `icons/`, `resources.qrc` | Toolbar icons, at 24 px and at 48 px (`@2x`) for high-DPI screens |

### `Chip8`: the interpreter

The machine state is held directly in the class:

| State | Details |
|---|---|
| Memory | 4 KB. The small font sits at `0x000` and the large SUPER-CHIP font at `0x050`; ROMs are loaded at `0x200`. |
| Registers | V0–VF, plus the 12-bit index register I |
| Program counter | PC |
| Stack | 16 entries, with stack pointer SP |
| Timers | Delay and sound timers |
| Display | One byte per pixel. 64×32, or 128×64 while `m_hiResMode` is set |
| Keys | The state of the 16 keypad keys |

**Naming.** CHIP-8 terms keep their documentation spelling (`PC`, `I`, `V`, `SP`, `ROM`), so the getters are `PC()`, `I()`, `V(x)` and `SP()`, and functions are named `loadROM()` and so on. Everything else follows Qt style: camelCase, no `get` prefix, `is`/`has` for booleans. Member variables carry an `m_` prefix (`m_PC`, `m_V[x]`, `m_delayTimer`), and constants are `SCREAMING_SNAKE_CASE`.

Main operations:

| Function | What it does |
|---|---|
| `loadROM(data, size)` | Checks that the ROM fits, then resets the machine and copies the ROM in. A ROM that is too large is rejected without touching the current state. |
| `step()` | Fetches, decodes and executes one opcode. Returns a `StepResult` (see below). |
| `tickTimers()` | Decrements the delay and sound timers. It must be called at 60 Hz. |
| `setKeyState()`, `isKeyPressed()` | Updates and reads the keypad state. |
| `hasError()`, `errorString()`, `error()`, `errorPC()`, `errorOpcode()` | Report errors. `errorString()` returns a `std::string`. |
| `PC()`, `I()`, `SP()`, `V(x)`, `delayTimer()`, `soundTimer()` | Read-only access to the CPU state, for the status bar and the error popup. |
| `displayBuffer()`, `displayWidth()`, `displayHeight()`, `isHiResMode()` | The frame buffer and its current resolution. |

Implementation notes:
- **FX0A (wait for key)** behaves like the COSMAC VIP: it waits for a key to be pressed *and released*. Until then, the instruction re-executes because PC is not advanced.
- **Memory access through I** wraps at 12 bits (`ADDR_MASK`), so a sprite or `FX55` near the end of memory never reads or writes outside the array.
- **Stack overflow** is not fatal. When the stack is full, a `CALL` drops the oldest return address. Some ROMs (for example Space Invaders) leave subroutines with a jump instead of `RET`, and would otherwise stop after a number of games. A `RET` with an empty stack is still reported as an error (`Error::StackUnderflow`).
- **`0NNN` (machine-code call)** is ignored, as in most interpreters. `0000` is treated as an invalid opcode, because it usually means the program ran into empty memory.

`step()` returns a `StepResult`, so the front-end knows what to update:

| Value | Meaning |
|---|---|
| `Ok` | Nothing visible changed |
| `GraphicsChanged` | The display buffer changed |
| `ModeToHigh` | `00FF` switched to 128×64. The display was cleared. |
| `ModeToLow` | `00FE` switched back to 64×32. The display was cleared. |
| `Exited` | The program ended with `00FD`. Further steps do nothing. |

### SUPER-CHIP instructions

| Opcode | What it does |
|---|---|
| `00FF` / `00FE` | Switch to high (128×64) or low (64×32) resolution. Sets `m_hiResMode` and clears the display. |
| `00CN` | Scroll the display down N pixels |
| `00FB` / `00FC` | Scroll the display right or left by 4 pixels |
| `00FD` | Exit. The emulator stops with "Program exited" in the status bar and the last frame on screen; Play starts the program again from the beginning. |
| `DXY0` | Draw a 16×16 sprite (2 bytes per row, 32 bytes in total) |
| `FX30` | Point I at the 8×10 large-font digit for Vx |
| `FX75` / `FX85` | Save V0..VX to, or load them from, the "RPL flags". These are a separate 16-byte store, cleared whenever a ROM is loaded or restarted with **Stop**. |

These SUPER-CHIP details follow Octo, the de-facto modern reference:
- Scrolling moves by pixels of the current resolution.
- `DXY0` draws 16×16 in both modes.
- The large font includes A–F.
- A collision sets VF to 1, not to the number of colliding rows as on the original HP48.

### Errors

Execution stops when the interpreter hits:
- an invalid opcode,
- a `RET` with an empty stack (stack underflow), or
- a program counter outside `0x200`–`0xFFE`.

`MainWindow` then stops the timer and the sound, shows the error in the status bar, and opens a popup. The popup shows the error type, the PC and opcode where it happened, SP, I, and V0–VF (in decimal).

### Quirks

CHIP-8 interpreters differ in a few details, and games depend on them. `Chip8::Quirks` makes these configurable:

| Quirk | On | Off |
|---|---|---|
| `shiftUsesVxOnly` | `8XY6`/`8XYE` shift Vx in place | Vy is copied into Vx first (VIP) |
| `loadStoreIncrementsI` | `FX55`/`FX65` advance I (VIP) | I is unchanged |
| `jumpUsesVx` | `BNNN` adds Vx (X = high nibble of NNN) | `BNNN` adds V0 (VIP) |
| `clipSprites` | Sprites are clipped at the screen edge | Sprites wrap around |
| `vfReset` | `8XY1/2/3` clear VF (VIP) | VF is unchanged |

The preferences dialog has two presets:
- **COSMAC VIP**: the original 1977 behaviour, needed by most early games.
- **CHIP-48 / SUPER-CHIP**: needed by most 1990s games.

A game that fails with one preset often runs with the other.

### Timing

Everything is driven by one 60 Hz tick, `MainWindow::onEmulationTick()`. Each tick:

1. **Decrements the timers.** The delay and sound timers count down once.
2. **Runs instructions.** It runs `m_instructionsPerSecond / 60` instructions. `m_instructionAllotment` carries the remainder over to the next tick, so speeds that aren't multiples of 60 still come out right on average.
3. **Updates the display and sound.** `handleStepResult()` refreshes the display after any instruction that changed it, and handles resolution switches. The beeper sounds while the sound timer is non-zero.

`QTimer` only accepts whole milliseconds, and a repeating 16 ms timer runs at 62.5 Hz. To get exactly 60 Hz on average, `m_cpuTimer` is a single-shot timer that `scheduleNextTick()` restarts against a fixed 1/60 s timeline, measured with `QElapsedTimer`. A late tick shortens the next wait, so the delays don't add up. If the emulator falls more than 100 ms behind (for example, because the event loop was blocked), the timeline resets instead of racing to catch up.

### Display and sound

**`DisplayWidget`** keeps its own copy of the frame buffer, together with its resolution. `setBuffer(buffer, width, height)` copies a frame of either size. The widget paints with the largest whole-number scale that fits the window, so pixels stay square and sharp, and centres the image. The scale is worked out in physical screen pixels (`devicePixelRatioF()`), so every CHIP-8 pixel is the same size even with fractional Windows scaling such as 125% or 150%.

**Themes.** The display colours come from a `DisplayTheme`, chosen in the preferences dialog:

| Theme | Pixels | Background |
|---|---|---|
| Orange on Black (default) | Orange `#FF8C00`, like old amber/orange monochrome monitors | Black |
| Green on Black | Green `#00DC5A` | Black |
| White on Black | White | Black |
| Black on White | Black | White |

The themes are listed once, in `displayThemes()` in `displaywidget.cpp`. To add a theme, add an enum value and a row there; the dialog picks it up automatically. The settings store the theme's key (for example `orangeOnBlack`) rather than its number, so reordering the list doesn't change anyone's saved choice.

**Resolution switches.** Both modes have the same 2:1 aspect ratio, so the window is never resized. When an instruction returns `ModeToHigh` or `ModeToLow`, `MainWindow::showDisplay()` copies the frame at the new resolution. `DisplayWidget` then rescales it to fit the same area, and the status bar shows the current resolution (`64x32` or `128x64`).

Loading a ROM or pressing **Stop** returns to low resolution.

**Fitting the window.** Because the scale is a whole number, a freely sized window would leave a margin around the image, drawn in the background colour. To avoid that, `fitWindowToDisplay()` shrinks the window after every resize, so the display area is an exact multiple of 128×64. The image then fills it exactly in both resolutions, because 64×32 is drawn at twice the 128×64 scale.
- On Windows, the fit happens when the user releases the window frame (`WM_EXITSIZEMOVE`, caught in `nativeEvent()`), so it never fights the mouse.
- Other resizes are fitted 200 ms after they settle, through `m_fitTimer`. That includes the first show of the window.
- Maximized and full-screen windows are left alone.

**`Beeper`** starts one `QAudioSink` stream and keeps it running. The stream reads from `SquareWaveGenerator`, which outputs silence while muted. Muting and unmuting only flips a flag, so the beep starts and stops without delay.

## Limitations

- **XO-CHIP is not supported.** That rules out 64 KB memory, colour planes, audio patterns, `00DN` scroll-up and `5XY2`/`5XY3`. XO-CHIP ROMs stop on an invalid opcode.
- **The VIP 64×64 two-page hires mode is not supported.**
- **Some SUPER-CHIP games written with Octo need the VIP quirk preset.** For example, octopeg and sub8 from the CHIP-8 Archive stop with an invalid opcode under the SUPER-CHIP preset, but run with the VIP one.
- **The maximum ROM size is 3584 bytes** (4096 − 0x200). Larger files are XO-CHIP programs and are rejected when loaded.

## Where to find ROMs

Most CHIP-8 software is public-domain homebrew, written from 1977 onwards. These are good places to find it. Keep in mind that this emulator runs **CHIP-8 and SUPER-CHIP**, but not XO-CHIP (see [Limitations](#limitations)).

### Game and program collections

- **[kripod/chip8-roms](https://github.com/kripod/chip8-roms)**: the classic "Chip-8 Program Pack" put together by Revival Studios.
  - It includes the well-known games (Pong, Tetris, Space Invaders, Brix, Blinky, …), demos and test programs.
  - Each ROM comes with a `.txt` file that gives its author, date and controls.
  - The `hires` folder holds the 64×64 VIP programs, which this emulator doesn't support (that mode is different from the SUPER-CHIP 128×64 mode).
- **[Zophar's Domain: CHIP-8 public-domain ROMs](https://www.zophar.net/pdroms/chip8.html)**: two classic downloads.
  - The *Chip-8 Games Pack* has 22 games, including Pong, Tetris, Invaders and Maze.
  - The *Super Chip Games Pack* (11 games, including Joust, Race and Alien) needs SUPER-CHIP support.
- **[CHIP-8 Archive](https://github.com/JohnEarnest/chip8Archive)** ([online gallery](https://johnearnest.github.io/chip8Archive/)): modern games, mostly entries from the yearly Octojam game jam, released under CC0.
  - CHIP-8 and SCHIP programs run here; the XO-CHIP ones don't.
  - `programs.json` lists the platform for each program.
- **[mattmikolay/chip-8](https://github.com/mattmikolay/chip-8)**: a few well-made programs under the MIT licence, including Cavern, Chipquarium, and the Delay Timer and Random Number tests.
- **[Octo on itch.io](https://internet-janitor.itch.io/octo)** and the Octojam pages: the IDE most modern CHIP-8 games are written in. Octojam entries are published on itch.io.

### Test ROMs (for checking the emulator)

- **[Timendus/chip8-test-suite](https://github.com/Timendus/chip8-test-suite)**: the most thorough test set available. Its tests cover:
  - the IBM logo, opcodes, flags,
  - **quirks**: this test shows which quirk settings are active, which helps check the presets,
  - the keypad, and sound.
  - It also has SCHIP tests, including a scrolling test, and XO-CHIP tests. The XO-CHIP tests will fail here.
- **[corax89/chip8-test-rom](https://github.com/corax89/chip8-test-rom)**: the `test_opcode.ch8` opcode test. It shows OK or an error for each group of instructions.

### Finding the right settings for a ROM

- **[chip-8/chip-8-database](https://github.com/chip-8/chip-8-database)**: metadata for known ROMs, looked up by their SHA-1 hash.
  - It lists each ROM's title, author, the platform it was written for, and the quirks it needs.
  - It's useful for deciding between the VIP and SUPER-CHIP presets, and could later be used to pick quirks automatically.
- **[tobiasvl/awesome-chip-8](https://github.com/tobiasvl/awesome-chip-8)**: a curated list of CHIP-8 resources, including documentation, emulators, tools and more ROM sources.

## License

This project is released under the [MIT License](LICENSE).

The ROMs mentioned above are not part of this repository and come with their own licences.
