#include "chip8.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>

namespace {
const uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// SUPER-CHIP 8x10 font for FX30. The original only had 0-9; A-F follow Octo.
const int     BIGFONT_ADDRESS = 0x50; // right after the small font
const uint8_t bigFontset[160] = {
    0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // 0
    0x18, 0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, // 1
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // 2
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 3
    0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, // 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 5
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 6
    0xFF, 0xFF, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, // 7
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 8
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 9
    0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, // A
    0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, // B
    0x3C, 0xFF, 0xC3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0xFF, 0x3C, // C
    0xFC, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFE, 0xFC, // D
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // E
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0  // F
};
}

Chip8::Chip8()
{
    // Seed rand() once from the clock, so CXNN differs from run to run and
    // games don't play out identically every time. Not cryptographic, and
    // doesn't need to be.
    static bool seeded = false;
    if (!seeded)
    {
        srand(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
        seeded = true;
    }

    reset(true);
}

void Chip8::reset(bool resetMemory)
{
    if (resetMemory)
    {
        memset(m_memory, 0, sizeof(m_memory));
        memcpy(m_memory, fontset, sizeof(fontset));
        memcpy(m_memory + BIGFONT_ADDRESS, bigFontset, sizeof(bigFontset));
        memset(m_rplFlags, 0, sizeof(m_rplFlags));
    }

    // reset all registers
    memset(m_V, 0, sizeof(m_V));
    m_I  = 0;
    m_PC = START_ADDRESS; // chip8 roms start at 0x200
    m_SP = 0;
    memset(m_stack, 0, sizeof(m_stack));
    memset(m_display, 0, sizeof(m_display));
    m_hiResMode  = false;
    m_delayTimer = 0;
    m_soundTimer = 0;
    memset(m_keys, 0, sizeof(m_keys));
    m_waitKey = -1;
    m_exited  = false;

    m_error       = Error::None;
    m_errorOpcode = 0;
    m_errorPC     = 0;
}

bool Chip8::loadROM(const uint8_t *data, size_t size)
{
    // check before resetting, so a rejected ROM leaves the current one intact
    if (size > static_cast<size_t>(MEM_SIZE - START_ADDRESS))
        return false;

    reset(true);
    if (size > 0)
        memcpy(m_memory + START_ADDRESS, data, size);
    return true;
}

void Chip8::tickTimers()
{
    if (m_delayTimer > 0)
        m_delayTimer--;
    if (m_soundTimer > 0)
        m_soundTimer--;
}

void Chip8::setKeyState(int key, bool pressed)
{
    if (key >= 0 && key < 16)
        m_keys[key] = pressed;
}

bool Chip8::isKeyPressed(uint8_t key) const
{
    return m_keys[key & 0x0F];
}

void Chip8::setError(Error e, uint16_t opcode, uint16_t pc)
{
    m_error       = e;
    m_errorOpcode = opcode;
    m_errorPC     = pc;
}

std::string Chip8::errorString() const
{
    char text[64];
    switch (m_error)
    {
        case Error::None:
            return std::string();
        case Error::InvalidOpcode:
            snprintf(text, sizeof(text), "Invalid opcode 0x%04x at 0x%04x", m_errorOpcode, m_errorPC);
            return text;
        case Error::StackUnderflow:
            snprintf(text, sizeof(text), "Return with an empty stack at 0x%04x", m_errorPC);
            return text;
        case Error::PCOutOfBounds:
            snprintf(text, sizeof(text), "Program counter out of bounds: 0x%04x", m_errorPC);
            return text;
    }
    return "Unknown error";
}

// Draws an 8 pixel wide sprite (1 byte per row) or, for DXY0, a 16 pixel wide
// one (2 bytes per row) at the current resolution.
bool Chip8::writeSpriteToDisplay(uint8_t x, uint8_t y, uint8_t height, uint8_t spriteWidth, uint16_t iReg)
{
    const int width       = displayWidth();
    const int heightPx    = displayHeight();
    const int bytesPerRow = spriteWidth / 8;
    bool      collision   = false;

    for (uint8_t row = 0; row < height; ++row)
    {
        uint16_t rowAddr    = iReg + row * bytesPerRow;
        uint16_t spriteBits = m_memory[rowAddr & ADDR_MASK];
        if (bytesPerRow == 2)
            spriteBits = (spriteBits << 8) | m_memory[(rowAddr + 1) & ADDR_MASK];
        int py = y + row;

        if (py >= heightPx)
        {
            if (m_quirks.clipSprites)
                continue;
            py %= heightPx;
        }

        for (uint8_t col = 0; col < spriteWidth; ++col)
        {
            if (!(spriteBits & (1u << (spriteWidth - 1 - col))))
                continue;

            int px = x + col;
            if (px >= width)
            {
                if (m_quirks.clipSprites)
                    continue;
                px %= width;
            }

            int idx = py * width + px;
            if (m_display[idx])
                collision = true;
            m_display[idx] ^= 1;
        }
    }
    return collision;
}

// Shifts the display contents by dx/dy pixels (at the current resolution);
// pixels shifted in from the edge are cleared.
void Chip8::scrollDisplay(int dx, int dy)
{
    const int width                       = displayWidth();
    const int height                      = displayHeight();
    uint8_t   scrolled[sizeof(m_display)] = {};

    for (int y = 0; y < height; ++y)
    {
        int srcY = y - dy;
        if (srcY < 0 || srcY >= height)
            continue;
        for (int x = 0; x < width; ++x)
        {
            int srcX = x - dx;
            if (srcX >= 0 && srcX < width)
                scrolled[y * width + x] = m_display[srcY * width + srcX];
        }
    }
    memcpy(m_display, scrolled, sizeof(m_display));
}

Chip8::StepResult Chip8::step()
{
    StepResult result = StepResult::Ok;

    // check PC
    if (m_exited)
        return StepResult::Exited;

    if (m_PC < 0x200 || m_PC >= MEM_SIZE - 1)
    {
        setError(Error::PCOutOfBounds, 0, m_PC);
        return result;
    }

    // fetch
    uint16_t opcode = (m_memory[m_PC] << 8) | m_memory[m_PC + 1];

    // decode and execute
    switch (opcode & 0xF000)
    {
        case 0x0000:
            switch (opcode)
            {
                case 0x00E0: // CLS: Clear the display
                    memset(m_display, 0, sizeof(m_display));
                    m_PC += 2;
                    result = StepResult::GraphicsChanged;
                    break;
                case 0x00FB: // SCR: scroll the display right by 4 pixels (SUPER-CHIP)
                    scrollDisplay(4, 0);
                    m_PC += 2;
                    result = StepResult::GraphicsChanged;
                    break;
                case 0x00FC: // SCL: scroll the display left by 4 pixels (SUPER-CHIP)
                    scrollDisplay(-4, 0);
                    m_PC += 2;
                    result = StepResult::GraphicsChanged;
                    break;
                case 0x00FD: // EXIT: stop the interpreter (SUPER-CHIP)
                    // the last frame stays on screen; the front-end stops running
                    m_exited = true;
                    result   = StepResult::Exited;
                    break;
                case 0x00FE: // LOW: switch to 64x32 (SUPER-CHIP)
                    m_hiResMode = false;
                    memset(m_display, 0, sizeof(m_display));
                    m_PC += 2;
                    result = StepResult::ModeToLow;
                    break;
                case 0x00FF: // HIGH: switch to 128x64 (SUPER-CHIP)
                    m_hiResMode = true;
                    memset(m_display, 0, sizeof(m_display));
                    m_PC += 2;
                    result = StepResult::ModeToHigh;
                    break;
                case 0x00EE: // RET: Return from subroutine
                    if (m_SP >= 1)
                    {
                        m_SP--;
                        m_PC = m_stack[m_SP]; // set PC to the return address
                    }
                    else
                    {
                        setError(Error::StackUnderflow, opcode, m_PC);
                    }
                    break;
                case 0x0000: // not a real instruction; usually means PC ran into empty memory
                    setError(Error::InvalidOpcode, opcode, m_PC);
                    break;
                default:
                    if ((opcode & 0xFFF0) == 0x00D0) // 00DN: XO-CHIP scroll up, not supported
                    {
                        setError(Error::InvalidOpcode, opcode, m_PC);
                        break;
                    }
                    if ((opcode & 0xFFF0) == 0x00C0) // SCD n: scroll the display down by n pixels (SUPER-CHIP)
                    {
                        scrollDisplay(0, opcode & 0x000F);
                        result = StepResult::GraphicsChanged;
                    }
                    // otherwise SYS addr: call to a COSMAC VIP machine code routine, ignored by interpreters
                    m_PC += 2;
                    break;
            }
            break;
        case 0x1000: // JP addr: Jump to location nnn
            m_PC = opcode & 0x0FFF;
            break;
        case 0x2000: // CALL addr: Call subroutine at nnn
            if (m_SP >= 16)
            {
                // Stack is full. Some ROMs (e.g. Space Invaders) leave subroutines with a
                // jump instead of RET, leaking one entry each time. Rather than stopping,
                // drop the oldest entry: those leaked addresses are never returned to.
                memmove(m_stack, m_stack + 1, sizeof(m_stack) - sizeof(m_stack[0]));
                m_SP--;
            }
            m_stack[m_SP] = m_PC + 2; // store return address
            m_SP++;
            m_PC = opcode & 0x0FFF;
            break;
        case 0x3000: // SE Vx, byte: Skip next instruction if Vx = kk
            if (m_V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
                m_PC += 4; // skip next instruction
            else
                m_PC += 2;
            break;
        case 0x4000: // SNE Vx, byte: Skip next instruction if Vx != kk
            if (m_V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
                m_PC += 4; // skip next instruction
            else
                m_PC += 2;
            break;
        case 0x5000:                    // SE Vx, Vy: Skip next instruction if Vx = Vy
            if ((opcode & 0x000F) != 0) // 5XY1-5XYF don't exist (5XY2/5XY3 are XO-CHIP)
                setError(Error::InvalidOpcode, opcode, m_PC);
            else if (m_V[(opcode & 0x0F00) >> 8] == m_V[(opcode & 0x00F0) >> 4])
                m_PC += 4; // skip next instruction
            else
                m_PC += 2;
            break;
        case 0x6000: // LD Vx, byte: Set Vx = kk
            m_V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
            m_PC += 2;
            break;
        case 0x7000: // ADD Vx, byte: Set Vx = Vx + kk
            m_V[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
            m_PC += 2;
            break;
        case 0x8000: // 8xyN: Arithmetic and logic operations
            switch (opcode & 0x000F)
            {
                case 0x0000: // LD Vx, Vy: Set Vx = Vy
                    m_V[(opcode & 0x0F00) >> 8] = m_V[(opcode & 0x00F0) >> 4];
                    m_PC += 2;
                    break;
                case 0x0001: // OR Vx, Vy: Set Vx = Vx OR Vy
                    m_V[(opcode & 0x0F00) >> 8] |= m_V[(opcode & 0x00F0) >> 4];
                    if (m_quirks.vfReset)
                        m_V[0xF] = 0;
                    m_PC += 2;
                    break;
                case 0x0002: // AND Vx, Vy: Set Vx = Vx AND Vy
                    m_V[(opcode & 0x0F00) >> 8] &= m_V[(opcode & 0x00F0) >> 4];
                    if (m_quirks.vfReset)
                        m_V[0xF] = 0;
                    m_PC += 2;
                    break;
                case 0x0003: // XOR Vx, Vy: Set Vx = Vx XOR Vy
                    m_V[(opcode & 0x0F00) >> 8] ^= m_V[(opcode & 0x00F0) >> 4];
                    if (m_quirks.vfReset)
                        m_V[0xF] = 0;
                    m_PC += 2;
                    break;
                case 0x0004: // ADD Vx, Vy: Set Vx = Vx + Vy, set VF = carry
                {
                    uint16_t sum                = uint16_t(m_V[(opcode & 0x0F00) >> 8]) + uint16_t(m_V[(opcode & 0x00F0) >> 4]);
                    m_V[(opcode & 0x0F00) >> 8] = sum & 0xFF;          // store result in Vx
                    m_V[0xF]                    = (sum > 255) ? 1 : 0; // set carry flag
                    m_PC += 2;
                }
                break;
                case 0x0005: // SUB Vx, Vy: Set Vx = Vx - Vy, set VF = NOT borrow
                {
                    uint8_t Vx                  = m_V[(opcode & 0x0F00) >> 8];
                    uint8_t Vy                  = m_V[(opcode & 0x00F0) >> 4];
                    m_V[(opcode & 0x0F00) >> 8] = Vx - Vy;            // store result in Vx
                    m_V[0xF]                    = (Vx >= Vy) ? 1 : 0; // set NOT borrow flag
                    m_PC += 2;
                }
                break;
                case 0x0006: // SHR Vx {, Vy}: Set Vx = Vx SHR 1
                {
                    uint8_t &vx = m_V[(opcode & 0x0F00) >> 8];
                    uint8_t  vy = m_V[(opcode & 0x00F0) >> 4];
                    if (!m_quirks.shiftUsesVxOnly)
                        vx = vy;
                    uint8_t carry = vx & 1;
                    vx >>= 1;         // store result in Vx
                    m_V[0xF] = carry; // store carry flag
                    m_PC += 2;
                }
                break;
                case 0x0007: // SUBN Vx, Vy: Set Vx = Vy - Vx, set VF = NOT borrow
                {
                    uint8_t Vx                  = m_V[(opcode & 0x0F00) >> 8];
                    uint8_t Vy                  = m_V[(opcode & 0x00F0) >> 4];
                    m_V[(opcode & 0x0F00) >> 8] = Vy - Vx;            // store result in Vx
                    m_V[0xF]                    = (Vy >= Vx) ? 1 : 0; // set NOT borrow flag
                    m_PC += 2;
                }
                break;
                case 0x000E: // SHL Vx {, Vy}: Set Vx = Vx SHL 1
                {
                    uint8_t &vx = m_V[(opcode & 0x0F00) >> 8];
                    uint8_t  vy = m_V[(opcode & 0x00F0) >> 4];
                    if (!m_quirks.shiftUsesVxOnly)
                        vx = vy;
                    uint8_t carry = (vx & 0x80) >> 7; // set carry flag
                    vx <<= 1;                         // store result in Vx
                    m_V[0xF] = carry;
                    m_PC += 2;
                }
                break;
                default:
                    setError(Error::InvalidOpcode, opcode, m_PC);
                    break;
            }
            break;
        case 0x9000:                    // SNE Vx, Vy: Skip next instruction if Vx != Vy
            if ((opcode & 0x000F) != 0) // 9XY1-9XYF don't exist
                setError(Error::InvalidOpcode, opcode, m_PC);
            else if (m_V[(opcode & 0x0F00) >> 8] != m_V[(opcode & 0x00F0) >> 4])
                m_PC += 4; // skip next instruction
            else
                m_PC += 2;
            break;
        case 0xA000: // LD I, addr: Set I = nnn
            m_I = opcode & 0x0FFF;
            m_PC += 2;
            break;
        case 0xB000: // JP V0, addr: Jump to location nnn + V0
            if (m_quirks.jumpUsesVx)
                m_PC = m_V[(opcode & 0x0F00) >> 8] + (opcode & 0x0FFF);
            else
                m_PC = m_V[0] + (opcode & 0x0FFF);
            break;
        case 0xC000: // RND Vx, byte: Set Vx = random byte AND kk
            m_V[(opcode & 0x0F00) >> 8] = (rand() % 256) & (opcode & 0x00FF);
            m_PC += 2;
            break;
        case 0xD000: // DRW Vx, Vy, nibble: Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision
        {
            // the start position wraps; the sprite itself is clipped or wrapped per quirk
            uint8_t x           = m_V[(opcode & 0x0F00) >> 8] % displayWidth();
            uint8_t y           = m_V[(opcode & 0x00F0) >> 4] % displayHeight();
            uint8_t height      = opcode & 0x000F;
            uint8_t spriteWidth = 8;
            if (height == 0) // DXY0: 16x16 sprite, 2 bytes per row (SUPER-CHIP)
            {
                height      = 16;
                spriteWidth = 16;
            }
            if (writeSpriteToDisplay(x, y, height, spriteWidth, m_I))
                m_V[0xF] = 1;
            else
                m_V[0xF] = 0;
            m_PC += 2;
            result = StepResult::GraphicsChanged;
        }
        break;
        case 0xE000: // SKP/SKNP Vx: Skip next instruction based on key state
            switch (opcode & 0x00FF)
            {
                case 0x9E: // SKP Vx
                    // Check if the key corresponding to Vx is pressed
                    // If pressed, skip the next instruction
                    if (isKeyPressed(m_V[(opcode & 0x0F00) >> 8]))
                        m_PC += 4; // skip next instruction
                    else
                        m_PC += 2;
                    break;
                case 0xA1: // SKNP Vx
                    // Check if the key corresponding to Vx is not pressed
                    // If not pressed, skip the next instruction
                    if (!isKeyPressed(m_V[(opcode & 0x0F00) >> 8]))
                        m_PC += 4; // skip next instruction
                    else
                        m_PC += 2;
                    break;
                default:
                    setError(Error::InvalidOpcode, opcode, m_PC);
                    break;
            }
            break;
        case 0xF000: // Miscellaneous and timer operations
            switch (opcode & 0x00FF)
            {
                case 0x07: // LD Vx, DT: Set Vx = delay timer value
                    m_V[(opcode & 0x0F00) >> 8] = m_delayTimer;
                    m_PC += 2;
                    break;
                case 0x0A: // LD Vx, K: Wait for a key press, store the value in Vx
                    // Like the COSMAC VIP, wait for a key to be pressed *and released*.
                    // Until then PC is left alone so this instruction re-executes.
                    if (m_waitKey < 0)
                    {
                        for (uint8_t i = 0; i < 16; ++i)
                        {
                            if (isKeyPressed(i))
                            {
                                m_waitKey = i;
                                break;
                            }
                        }
                    }
                    else if (!isKeyPressed(m_waitKey))
                    {
                        m_V[(opcode & 0x0F00) >> 8] = m_waitKey;
                        m_waitKey                   = -1;
                        m_PC += 2;
                    }
                    break;
                case 0x15: // LD DT, Vx: Set delay timer = Vx
                    m_delayTimer = m_V[(opcode & 0x0F00) >> 8];
                    m_PC += 2;
                    break;
                case 0x18: // LD ST, Vx: Set sound timer = Vx
                    m_soundTimer = m_V[(opcode & 0x0F00) >> 8];
                    m_PC += 2;
                    break;
                case 0x1E: // ADD I, Vx: Set I = I + Vx
                    m_I += m_V[(opcode & 0x0F00) >> 8];
                    m_PC += 2;
                    break;
                case 0x29:                                          // LD F, Vx: Set I = location of sprite for digit Vx
                    m_I = (m_V[(opcode & 0x0F00) >> 8] & 0x0F) * 5; // Each sprite is 5 bytes long, only the low nibble counts
                    m_PC += 2;
                    break;
                case 0x30: // LD HF, Vx: Set I = location of the 8x10 sprite for digit Vx (SUPER-CHIP)
                    m_I = BIGFONT_ADDRESS + (m_V[(opcode & 0x0F00) >> 8] & 0x0F) * 10;
                    m_PC += 2;
                    break;
                case 0x75: // LD R, Vx: Store V0 through Vx in the RPL flags (SUPER-CHIP)
                    memcpy(m_rplFlags, m_V, ((opcode & 0x0F00) >> 8) + 1);
                    m_PC += 2;
                    break;
                case 0x85: // LD Vx, R: Read V0 through Vx from the RPL flags (SUPER-CHIP)
                    memcpy(m_V, m_rplFlags, ((opcode & 0x0F00) >> 8) + 1);
                    m_PC += 2;
                    break;
                case 0x33: // LD B, Vx: Store BCD representation of Vx in memory locations I, I+1, and I+2
                {
                    uint8_t value                   = m_V[(opcode & 0x0F00) >> 8];
                    m_memory[m_I & ADDR_MASK]       = value / 100;       // Hundreds place
                    m_memory[(m_I + 1) & ADDR_MASK] = (value / 10) % 10; // Tens place
                    m_memory[(m_I + 2) & ADDR_MASK] = value % 10;        // Ones place
                    m_PC += 2;
                }
                break;
                case 0x55: // LD [I], Vx: Store registers V0 through Vx in memory starting at location I
                {
                    uint8_t x = (opcode & 0x0F00) >> 8;
                    for (uint8_t i = 0; i <= x; ++i)
                        m_memory[(m_I + i) & ADDR_MASK] = m_V[i];
                    if (m_quirks.loadStoreIncrementsI)
                        m_I += x + 1;
                    m_PC += 2;
                }
                break;
                case 0x65: // LD Vx, [I]: Read registers V0 through Vx from memory starting at location I
                {
                    uint8_t x = (opcode & 0x0F00) >> 8;
                    for (uint8_t i = 0; i <= x; ++i)
                        m_V[i] = m_memory[(m_I + i) & ADDR_MASK];
                    if (m_quirks.loadStoreIncrementsI)
                        m_I += x + 1;
                    m_PC += 2;
                }
                break;
                default:
                    setError(Error::InvalidOpcode, opcode, m_PC);
                    break;
            }
            break;
        default:
            setError(Error::InvalidOpcode, opcode, m_PC);
            break;
    }

    return result;
}
