#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Shadowdark Torch — parametros ajustables
// Todo lo que quieras calibrar en mesa esta en este archivo.
// ---------------------------------------------------------------------------

// --- Duracion de la antorcha ----------------------------------------------
// Shadowdark: una antorcha dura 1 hora de tiempo real.
static constexpr uint32_t TORCH_DURATION_MS = 60UL * 60UL * 1000UL;  // 60 min

// La barra se divide en 200 fragmentos -> 1 fragmento = 18 s.
static constexpr int      BAR_SEGMENTS   = 200;
static constexpr uint32_t MS_PER_SEGMENT = TORCH_DURATION_MS / BAR_SEGMENTS;

// --- Refresco de pantalla --------------------------------------------------
static constexpr uint32_t FRAME_INTERVAL_MS = 250;   // 4 fps, anima el fuego
static constexpr uint8_t  SCREEN_BRIGHTNESS = 160;   // 0-255

// --- Encendido por agitacion (acelerometro) --------------------------------
// Para que un golpe a la mesa NO encienda la antorcha se exige una sacudida
// real: varios picos fuertes seguidos dentro de una ventana de tiempo.
static constexpr float    SHAKE_PEAK_G       = 1.60f; // g por encima de 1g en reposo
static constexpr uint8_t  SHAKE_PEAKS_NEEDED = 3;     // picos para encender
static constexpr uint32_t SHAKE_PEAK_GAP_MS  = 70;    // separacion minima entre picos
static constexpr uint32_t SHAKE_WINDOW_MS    = 1500;  // ventana para acumularlos
static constexpr uint32_t IMU_POLL_MS        = 10;    // 100 Hz

// --- Apagado soplando (microfono) ------------------------------------------
// El umbral es adaptativo: se compara con el ruido ambiente medido en vivo,
// asi funciona igual en una mesa silenciosa que en un bar ruidoso.
static constexpr uint32_t MIC_SAMPLE_RATE   = 16000;
static constexpr size_t   MIC_BLOCK_SAMPLES = 256;    // 16 ms por bloque
static constexpr float    BLOW_FLOOR_RATIO  = 6.0f;   // veces sobre el ruido ambiente
static constexpr float    BLOW_ABS_MIN_RMS  = 1800.0f;// suelo absoluto (RMS int16)
static constexpr uint32_t BLOW_SUSTAIN_MS   = 320;    // hay que soplar sostenido

// --- Anti-rebotes ----------------------------------------------------------
static constexpr uint32_t TOUCH_DEBOUNCE_MS = 300;
// Tras apagarse, ignora sensores un momento (el mismo soplido/sacudida no
// debe volver a encenderla en el acto).
static constexpr uint32_t RELIGHT_LOCKOUT_MS = 1500;

// --- Opciones de comportamiento -------------------------------------------
// Pantalla en negro: ademas de pintar de negro, apaga la retroiluminacion
// para no iluminar la mesa. Ponlo en false si prefieres solo pintar negro.
static constexpr bool BLACKOUT_TURNS_OFF_BACKLIGHT = true;

// El contador sigue corriendo en tiempo real con la pantalla en negro.
// Ponlo en true si prefieres que la pantalla en negro congele el tiempo.
static constexpr bool BLACKOUT_PAUSES_TIMER = false;

// Al soplar, la antorcha se apaga por completo y la proxima sacudida arranca
// una antorcha nueva de 60 min. Ponlo en true si en tu mesa preferis que
// soplar solo "guarde" la antorcha y al reencender siga el tiempo restante.
static constexpr bool RESUME_AFTER_BLOWOUT = false;

// --- Geometria de pantalla (CoreS3: 320x240) -------------------------------
static constexpr int SCREEN_W = 320;
static constexpr int SCREEN_H = 240;

static constexpr int   ART_SCALE  = 7;    // 1 pixel de arte = 7 px de pantalla
static constexpr int   ART_TOP_Y  = 6;    // borde superior del dibujo

static constexpr float BAR_WIDTH_RATIO = 0.85f;                       // 85% del ancho
static constexpr int   BAR_W = (int)(SCREEN_W * BAR_WIDTH_RATIO);     // 272 px
static constexpr int   BAR_X = (SCREEN_W - BAR_W) / 2;                // 24
static constexpr int   BAR_Y = 196;
static constexpr int   BAR_H = 26;
