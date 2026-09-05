#pragma once
#include <M5Unified.h>

// ---------------------------------------------------------------------------
// Pixel art de la antorcha
// Rejilla de 16 columnas. Cada fotograma de llama son 13 filas y el cuerpo
// (trapo + mango) otras 13 -> 26 filas en total.
//
// Paleta (un caracter = un pixel):
//   .  transparente      W  blanco incandescente   Y  amarillo
//   O  naranja           R  rojo                   S  humo
//   B  trapo oscuro      b  trapo claro
//   H  madera oscura     h  madera clara
//   K  carbon oscuro     k  carbon claro
// ---------------------------------------------------------------------------

static constexpr int ART_COLS       = 16;
static constexpr int ART_FLAME_ROWS = 13;
static constexpr int ART_BODY_ROWS  = 13;
static constexpr int ART_ROWS       = ART_FLAME_ROWS + ART_BODY_ROWS;  // 26
static constexpr int ART_FRAMES     = 4;

// Fotogramas de la llama: recta, inclinada a la izquierda, a la derecha y baja.
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

// Cabeza apagada: el trapo carbonizado cuando no hay fuego.
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

// Cuerpo fijo: trapo atado y mango de madera.
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

// --- Paleta ----------------------------------------------------------------
struct Rgb {
    uint8_t r, g, b;
};

// Devuelve false si el caracter es transparente.
inline bool artColor(char c, Rgb& out) {
    switch (c) {
        case 'W': out = {255, 246, 205}; return true;  // nucleo incandescente
        case 'Y': out = {255, 203,  64}; return true;  // amarillo
        case 'O': out = {243, 138,  28}; return true;  // naranja
        case 'R': out = {197,  55,  16}; return true;  // rojo
        case 'S': out = {110, 108, 104}; return true;  // humo
        case 'B': out = { 62,  50,  44}; return true;  // trapo oscuro
        case 'b': out = { 96,  79,  68}; return true;  // trapo claro
        case 'H': out = { 72,  47,  26}; return true;  // madera oscura
        case 'h': out = {112,  77,  42}; return true;  // madera clara
        case 'K': out = { 44,  40,  38}; return true;  // carbon
        case 'k': out = { 60,  55,  52}; return true;  // carbon claro
        default:  return false;                        // '.' transparente
    }
}

// Multiplica un color por un factor (para el parpadeo de la llama).
inline Rgb dimRgb(const Rgb& c, float f) {
    auto ch = [f](uint8_t v) -> uint8_t {
        int r = (int)(v * f + 0.5f);
        return (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
    };
    return {ch(c.r), ch(c.g), ch(c.b)};
}
