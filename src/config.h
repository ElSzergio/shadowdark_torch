#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Shadowdark Torch — tunable parameters
// Everything you might want to calibrate at the table lives in this file.
// ---------------------------------------------------------------------------

// --- Torch duration --------------------------------------------------------
// Shadowdark: a torch lasts 1 hour of real time.
static constexpr uint32_t TORCH_DURATION_MS = 60UL * 60UL * 1000UL;  // 60 min

// The bar is split into 200 segments -> 1 segment = 18 s.
static constexpr int      BAR_SEGMENTS   = 200;
static constexpr uint32_t MS_PER_SEGMENT = TORCH_DURATION_MS / BAR_SEGMENTS;

// --- Display refresh -------------------------------------------------------
static constexpr uint32_t FRAME_INTERVAL_MS = 250;   // 4 fps, animates the fire
static constexpr uint8_t  SCREEN_BRIGHTNESS = 160;   // 0-255

// --- Shake to light (accelerometer) ----------------------------------------
// So a knock on the table does NOT light the torch, a real shake is required:
// several strong peaks in a row within a time window.
static constexpr float    SHAKE_PEAK_G       = 1.60f; // g above the 1g at rest
static constexpr uint8_t  SHAKE_PEAKS_NEEDED = 3;     // peaks needed to light
static constexpr uint32_t SHAKE_PEAK_GAP_MS  = 70;    // minimum gap between peaks
static constexpr uint32_t SHAKE_WINDOW_MS    = 1500;  // window to collect them
static constexpr uint32_t IMU_POLL_MS        = 10;    // 100 Hz

// --- Blow to snuff (microphone) --------------------------------------------
// The threshold is adaptive: it is compared against the ambient noise measured
// live, so it works the same at a quiet table and in a noisy bar.
static constexpr uint32_t MIC_SAMPLE_RATE   = 16000;
static constexpr size_t   MIC_BLOCK_SAMPLES = 256;    // 16 ms per block
static constexpr float    BLOW_FLOOR_RATIO  = 6.0f;   // times above ambient noise
static constexpr float    BLOW_ABS_MIN_RMS  = 1800.0f;// absolute floor (int16 RMS)
static constexpr uint32_t BLOW_SUSTAIN_MS   = 640;    // the blow must be sustained

// --- Battery dots ----------------------------------------------------------
// One grey dot per 5% of charge; they go out one by one as the battery drains.
static constexpr int      BATTERY_DOTS     = 20;
static constexpr uint32_t BATTERY_POLL_MS  = 10000;  // the fuel gauge barely moves
// A spent dot only comes back once the charge is this far past its edge, so a
// reading wobbling on a boundary does not make the dot blink.
static constexpr int      BATTERY_HYST_PCT = 2;

// --- Debouncing ------------------------------------------------------------
static constexpr uint32_t TOUCH_DEBOUNCE_MS = 300;
// After going out, ignore the sensors for a moment (the same blow or shake
// must not light it straight back up).
static constexpr uint32_t RELIGHT_LOCKOUT_MS = 1500;

// --- Behaviour options -----------------------------------------------------
// On lighting, blowing starts locked out: you must tap the screen to arm it.
// That way no table noise can put the torch out by accident. Set it to false
// if you would rather it be born armed and the tap be what protects it.
static constexpr bool BLOW_LOCKED_ON_LIGHT = true;

// When blown out, the torch is spent entirely and the next shake starts a
// fresh 60 min torch. Set this to true if at your table blowing should merely
// "stow" the torch and relighting should resume the remaining time.
static constexpr bool RESUME_AFTER_BLOWOUT = false;

// --- Screen geometry (CoreS3: 320x240) -------------------------------------
static constexpr int SCREEN_W = 320;
static constexpr int SCREEN_H = 240;

static constexpr int   ART_SCALE  = 7;    // 1 art pixel = 7 screen px
static constexpr int   ART_TOP_Y  = 6;    // top edge of the drawing

static constexpr float BAR_WIDTH_RATIO = 0.85f;                       // 85% of the width
static constexpr int   BAR_W = (int)(SCREEN_W * BAR_WIDTH_RATIO);     // 272 px
static constexpr int   BAR_X = (SCREEN_W - BAR_W) / 2;                // 24
static constexpr int   BAR_Y = 190;
static constexpr int   BAR_H = 24;

// Padlock, centred below the bar.
static constexpr int   LOCK_SCALE = 3;
static constexpr int   LOCK_Y     = 216;

// Battery dots, on the padlock row: 10 to its left, 10 to its right, spanning
// the bar's width. Level with the padlock's body rather than tucked under the
// bar, so they do not read as tick marks of the torch bar.
static constexpr int   BATTERY_DOT_SIZE  = 3;    // = LOCK_SCALE, one padlock pixel
static constexpr int   BATTERY_DOT_PITCH = 12;
static constexpr int   BATTERY_DOT_Y     = 231;
