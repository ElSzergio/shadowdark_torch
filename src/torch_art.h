#pragma once
#include <M5Unified.h>

// ---------------------------------------------------------------------------
// Torch pixel art
// A 16-column grid. Each flame frame is 13 rows and the body (rag + handle)
// another 13 -> 26 rows in total.
//
// Palette (one character = one pixel):
//   .  transparent       W  white hot              Y  yellow
//   O  orange            R  red                    S  smoke
//   B  dark rag          b  light rag
//   H  dark wood         h  light wood
//   K  dark charcoal     k  light charcoal
// ---------------------------------------------------------------------------

static constexpr int ART_COLS       = 16;
static constexpr int ART_FLAME_ROWS = 13;
static constexpr int ART_BODY_ROWS  = 13;
static constexpr int ART_ROWS       = ART_FLAME_ROWS + ART_BODY_ROWS;  // 26
static constexpr int ART_FRAMES     = 4;

// Flame frames: upright, leaning left, leaning right, and low.
static const char* const TORCH_FLAME[ART_FRAMES][ART_FLAME_ROWS] = {
    {
        "................",
        ".......RR.......",
        "......ROOR......",
        "......ROOR......",
        ".....ROYYOR.....",
        ".....ROYYOR.....",
        "....ROYWWYOR....",
        "....ROYWWYOR....",
        "...ROYWWWWYOR...",
        "...ROYWWWWYOR...",
        "...ROYYWWYYOR...",
        "....ROYYYYOR....",
        ".....ROOOOR.....",
    },
    {
        "......RR........",
        "......ROR.......",
        ".....ROOR.......",
        ".....ROYOR......",
        "....ROYYOR......",
        "....ROYWYOR.....",
        "...ROYWWYOR.....",
        "...ROYWWWYOR....",
        "...ROYWWWWYOR...",
        "...ROYWWWWYOR...",
        "...ROYYWWYYOR...",
        "....ROYYYYOR....",
        ".....ROOOOR.....",
    },
    {
        "................",
        "........RR......",
        ".......ROOR.....",
        ".......ROOR.....",
        "......ROYYOR....",
        "......ROYYOR....",
        ".....ROYWWYOR...",
        "....ROYWWWYOR...",
        "...ROYWWWWYOR...",
        "...ROYWWWWYOR...",
        "...ROYYWWYYOR...",
        "....ROYYYYOR....",
        ".....ROOOOR.....",
    },
    {
        "................",
        "................",
        ".......RR.......",
        "......ROOR......",
        "......ROYOR.....",
        ".....ROYYOR.....",
        ".....ROYWYOR....",
        "....ROYWWWYOR...",
        "...ROYWWWWYOR...",
        "...ROYWWWWYOR...",
        "...ROYYWWYYOR...",
        "....ROYYYYOR....",
        ".....ROOOOR.....",
    },
};

// Snuffed head: the charred rag when there is no fire.
static const char* const TORCH_HEAD_OUT[ART_FLAME_ROWS] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "......KKKK......",
    ".....KKkkKK.....",
    "....KKkKKkKK....",
};

// Fixed body: tied rag and wooden handle.
static const char* const TORCH_BODY[ART_BODY_ROWS] = {
    ".....BBBBBB.....",
    "....BbBBBBbB....",
    "....BBbBBbBB....",
    ".....BBBBBB.....",
    "......HhhH......",
    "......HhhH......",
    "......HhHH......",
    "......HhhH......",
    "......HhhH......",
    "......HHhH......",
    "......HhhH......",
    "......HhhH......",
    ".....HHhhHH.....",
};

// --- Padlock ---------------------------------------------------------------
// Shows whether blowing can put the torch out. Closed = safe from accidents,
// open = the next blow snuffs it. It is drawn in a flat colour, so only the
// silhouette is needed here.
static constexpr int LOCK_COLS = 8;
static constexpr int LOCK_ROWS = 8;

static const char* const LOCK_CLOSED[LOCK_ROWS] = {
    "..####..",
    ".##..##.",
    ".##..##.",
    "########",
    "###..###",
    "###..###",
    "########",
    "########",
};

// The shackle open, swung to the right and clear of the body.
static const char* const LOCK_OPEN[LOCK_ROWS] = {
    "...####.",
    "..##..##",
    "......##",
    "########",
    "###..###",
    "###..###",
    "########",
    "########",
};

// --- Palette ---------------------------------------------------------------
struct Rgb {
    uint8_t r, g, b;
};

// Returns false if the character is transparent.
inline bool artColor(char c, Rgb& out) {
    switch (c) {
        case 'W': out = {255, 246, 205}; return true;  // white-hot core
        case 'Y': out = {255, 203,  64}; return true;  // yellow
        case 'O': out = {243, 138,  28}; return true;  // orange
        case 'R': out = {197,  55,  16}; return true;  // red
        case 'S': out = {110, 108, 104}; return true;  // smoke
        case 'B': out = { 62,  50,  44}; return true;  // dark rag
        case 'b': out = { 96,  79,  68}; return true;  // light rag
        case 'H': out = { 72,  47,  26}; return true;  // dark wood
        case 'h': out = {112,  77,  42}; return true;  // light wood
        case 'K': out = { 44,  40,  38}; return true;  // charcoal
        case 'k': out = { 60,  55,  52}; return true;  // light charcoal
        default:  return false;                        // '.' transparent
    }
}

// Multiplies a colour by a factor (for the flame flicker).
inline Rgb dimRgb(const Rgb& c, float f) {
    auto ch = [f](uint8_t v) -> uint8_t {
        int r = (int)(v * f + 0.5f);
        return (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
    };
    return {ch(c.r), ch(c.g), ch(c.b)};
}
