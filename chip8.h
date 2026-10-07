#ifndef CHIP8_H
#define CHIP8_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

// Core CHIP-8 / SUPER-CHIP interpreter: memory, registers, the instruction
// set, the 64x32 and 128x64 displays, error reporting and the behaviour
// "quirks" that differ between classic CHIP-8 and later interpreters.
// Has no GUI dependencies; the front-end drives it one instruction at a time.
//
// Naming: CHIP-8 terms keep their documentation spelling (PC, I, V, SP, ROM),
// so the getters are PC(), I(), V(x) and SP(). Members carry an m_ prefix.

class Chip8
{
public:
    static const int MEM_SIZE      = 4096;
    static const int LORES_WIDTH   = 64; // low resolution (CHIP-8)
    static const int LORES_HEIGHT  = 32;
    static const int HIRES_WIDTH   = 128; // high resolution (SUPER-CHIP 00FF)
    static const int HIRES_HEIGHT  = 64;
    static const int START_ADDRESS = 0x200;
    static const int ADDR_MASK     = MEM_SIZE - 1; // wrap memory accesses to 12 bits

    enum class Error
    {
        None,
        InvalidOpcode,
        StackUnderflow, // RET with an empty stack (a full stack drops its oldest entry instead)
        PCOutOfBounds
    };

    // Behavior quirks that vary between CHIP-8 implementations/games.
    struct Quirks
    {
        bool shiftUsesVxOnly      = false; // 8XY6/8XYE shift Vx in place (don't copy Vy into Vx first)
        bool loadStoreIncrementsI = true;  // FX55/FX65 increment I as a side effect (COSMAC VIP behavior)
        bool jumpUsesVx           = false; // BNNN uses Vx (per high nibble) instead of V0 (Super-CHIP behavior)
        bool clipSprites          = true;  // sprites are clipped at the screen edge instead of wrapping
        bool vfReset              = true;  // 8XY1/8XY2/8XY3 set VF to 0 (COSMAC VIP behavior)

        // Presets. The defaults above are the original COSMAC VIP behavior.
        static Quirks cosmacVip() { return Quirks(); }
        static Quirks superChip() // CHIP-48 / SUPER-CHIP, e.g. most David Winter games
        {
            Quirks q;
            q.shiftUsesVxOnly      = true;
            q.loadStoreIncrementsI = false;
            q.jumpUsesVx           = true;
            q.clipSprites          = true;
            q.vfReset              = false;
            return q;
        }
    };

    Chip8();

    void reset(bool resetMemory);
    // Copies a ROM image of `size` bytes to 0x200 after a full reset. Returns
    // false, without touching the current state, if it doesn't fit in memory.
    bool loadROM(const uint8_t *data, size_t size);

    // What executing an instruction did, so the front-end knows what to update.
    enum class StepResult
    {
        Ok,              // nothing visible changed
        GraphicsChanged, // the display buffer changed
        ModeToHigh,      // switched to 128x64 (00FF); the display was cleared
        ModeToLow,       // switched to 64x32 (00FE); the display was cleared
        Exited           // the program ended with 00FD; further steps do nothing
    };

    // Executes one instruction.
    StepResult step();

    // Should be called at ~60Hz to decrement delay/sound timers.
    void tickTimers();

    void setKeyState(int key, bool pressed);
    bool isKeyPressed(uint8_t key) const;

    bool        hasExited() const { return m_exited; }
    bool        hasError() const { return m_error != Error::None; }
    Error       error() const { return m_error; }
    std::string errorString() const; // empty when there is no error
    uint16_t    errorPC() const { return m_errorPC; } // address of the failing instruction
    uint16_t    errorOpcode() const { return m_errorOpcode; }

    // The buffer holds displayWidth() x displayHeight() pixels, one byte each, row by row.
    const uint8_t *displayBuffer() const { return m_display; }
    bool           isHiResMode() const { return m_hiResMode; }
    int            displayWidth() const { return m_hiResMode ? HIRES_WIDTH : LORES_WIDTH; }
    int            displayHeight() const { return m_hiResMode ? HIRES_HEIGHT : LORES_HEIGHT; }

    void   setQuirks(const Quirks &q) { m_quirks = q; }
    Quirks quirks() const { return m_quirks; }

    // CPU state, named as in the CHIP-8 documentation
    uint16_t PC() const { return m_PC; }
    uint16_t I() const { return m_I; }
    uint8_t  SP() const { return m_SP; }
    uint8_t  V(unsigned int x) const
    {
        assert(x < 16);
        return x < 16 ? m_V[x] : 0;
    }
    uint8_t delayTimer() const { return m_delayTimer; }
    uint8_t soundTimer() const { return m_soundTimer; }

private:
    uint8_t  m_memory[MEM_SIZE];
    uint8_t  m_V[16];
    uint16_t m_I;
    uint16_t m_PC;
    uint8_t  m_SP;
    uint16_t m_stack[16];
    uint8_t  m_delayTimer;
    uint8_t  m_soundTimer;
    uint8_t  m_display[HIRES_WIDTH * HIRES_HEIGHT]; // sized for hires; lores uses the first 64x32
    bool     m_hiResMode;                           // SUPER-CHIP 128x64 mode, switched by 00FF / 00FE
    uint8_t  m_rplFlags[16];                        // SUPER-CHIP FX75/FX85 storage (the HP48 "RPL user flags")
    bool     m_keys[16];
    int8_t   m_waitKey; // FX0A: key pressed while waiting, -1 if none yet
    bool     m_exited;  // 00FD was executed

    Error    m_error;
    uint16_t m_errorOpcode;
    uint16_t m_errorPC;

    Quirks m_quirks;

    void setError(Error e, uint16_t opcode, uint16_t pc);
    bool writeSpriteToDisplay(uint8_t x, uint8_t y, uint8_t height, uint8_t spriteWidth, uint16_t iReg);
    void scrollDisplay(int dx, int dy);
};

#endif // CHIP8_H
