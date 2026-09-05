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
// Un soplido tiene que cumplir TRES cosas a la vez. Con volumen y duracion
// solos, una frase dicha cerca del aparato apagaba la antorcha.
//
//   1. Grave: el soplido es turbulencia de aire, casi toda su energia esta
//      por debajo de ~120 Hz. La voz vive muy por encima. Este es el filtro
//      que de verdad separa soplar de hablar.
//   2. Fuerte: soplar sobre el microfono lo satura; hablar a un palmo rara
//      vez pasa del 3-4% del fondo de escala.
//   3. Sostenido: medio segundo. Descarta plosivas ("p", "t"), palmadas y
//      golpes, que son grave y fuerte pero duran 50 ms.
static constexpr uint32_t MIC_SAMPLE_RATE   = 16000;
static constexpr size_t   MIC_BLOCK_SAMPLES = 256;    // 16 ms por bloque

// (1) Paso bajo de dos polos. alpha = 1 - exp(-2*pi*fc/fs), aqui fc ~ 120 Hz.
static constexpr float    MIC_LP_ALPHA      = 0.046f;
// Cuanto debe pesar lo grave frente al total. Medido en simulacion:
// soplido 1.3-1.5, palmada/golpe 1.0-1.4 (pero duran 100 ms), voz 0.11-0.21
// incluso gritando. El margen entre soplar y hablar es enorme.
static constexpr float    BLOW_LF_RATIO_MIN = 0.60f;

// (2) Nivel minimo, en % del fondo de escala del microfono.
// 5%: hablar cerca se queda en 1.5%, pero gritar llega al 10%, asi que el
// nivel por si solo no distingue voz de soplido. Quien lo distingue es el
// filtro de graves; este umbral esta para descartar retumbes lejanos.
static constexpr float    BLOW_MIN_LEVEL_PCT = 5.0f;
static constexpr float    BLOW_ABS_MIN_RMS   = 32767.0f * BLOW_MIN_LEVEL_PCT / 100.0f;
// Ademas, tantas veces por encima del ruido ambiente medido en vivo. Bajo a
// proposito: el filtro de graves ya hace la criba fina, y un multiplo alto
// aqui llegaba a tapar el propio soplido.
static constexpr float    BLOW_FLOOR_RATIO   = 3.0f;
// El nivel se alisa antes de comparar (~40 ms). Un soplido es casi todo grave
// y a bloques de 16 ms su volumen baila mucho; sin alisar, el contador de
// "sostenido" se reseteaba solo y no apagaba nunca.
static constexpr float    MIC_LEVEL_SMOOTH   = 0.35f;

// (3) Cuanto hay que sostenerlo, y a que ritmo se vacia el contador cuando
// el sonido para. Vaciar mas rapido de lo que se llena evita que una serie de
// golpes ritmicos en la mesa acabe sumando medio segundo.
static constexpr uint32_t BLOW_SUSTAIN_MS   = 600;
static constexpr float    BLOW_DECAY_MULT   = 1.5f;

// --- Anti-rebotes ----------------------------------------------------------
static constexpr uint32_t TOUCH_DEBOUNCE_MS = 300;
// Tras apagarse, ignora sensores un momento (el mismo soplido/sacudida no
// debe volver a encenderla en el acto).
static constexpr uint32_t RELIGHT_LOCKOUT_MS = 1500;

// --- Opciones de comportamiento -------------------------------------------
// Diagnostico: escribe bajo la barra por que se apago y a los cuantos minutos
// ("OUT AT 15:23 - SNUFFED"), y el motivo del ultimo arranque. Ponlo en false
// cuando ya confies en la antorcha.
static constexpr bool SHOW_OUT_DEBUG = true;

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
