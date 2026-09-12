// ---------------------------------------------------------------------------
//  SHADOWDARK TORCH  —  M5Stack CoreS3
// ---------------------------------------------------------------------------
//  A Shadowdark torch lasts 1 hour of real time. This project keeps track of
//  it for you:
//
//    * LIGHT   : shake the CoreS3 like you mean it (several strong peaks in a
//                row; a knock on the table is not enough).
//    * TIMER   : a 200-segment bar at 85% of the width, 60 minutes. The
//                minutes are never shown, only the bar.
//    * SNUFF   : blow hard over the microphones, or wait for it to burn down.
//                Once spent, you have to light a new one from scratch.
//    * PADLOCK : blowing only snuffs it while armed. A screen tap arms and
//                disarms it; a fresh torch is born locked, so no table noise
//                can put it out by accident.
//    * BATTERY : 20 grey dots beside the padlock, one per 5% of charge.
//
//  The view refreshes every 250 ms, animating the flame.
// ---------------------------------------------------------------------------

#include <M5Unified.h>

#include <cmath>
#include <cstdlib>

#include "config.h"
#include "torch_art.h"

// Set to 1 to dump the microphone level over serial and calibrate the blow.
#define MIC_DEBUG 0

namespace {

// --- State -----------------------------------------------------------------
enum class State : uint8_t { Unlit, Burning, Out };
enum class OutReason : uint8_t { Expired, Blown };

State     g_state      = State::Unlit;
OutReason g_out_reason = OutReason::Expired;

uint32_t g_burn_start_ms      = 0;  // moment it was lit (adjusted for pauses)
uint32_t g_burn_budget_ms     = 0;  // duration of this torch
uint32_t g_saved_remaining_ms = 0;  // time stored on blowing (if it resumes)
uint32_t g_out_since_ms       = 0;  // when it went out (for the smoke)
uint32_t g_lockout_until_ms   = 0;  // ignore sensors right after going out

// Blow padlock: while it is on, blowing does not put the torch out.
bool     g_blow_locked   = BLOW_LOCKED_ON_LIGHT;
uint32_t g_last_touch_ms = 0;

uint32_t g_last_frame_ms = 0;
uint32_t g_frame_counter = 0;

// --- Canvas ----------------------------------------------------------------
M5Canvas   g_canvas(&M5.Display);
lgfx::LovyanGFX* g_gfx        = nullptr;
bool       g_use_canvas = false;

// --- Derived geometry ------------------------------------------------------
constexpr int ART_W    = ART_COLS * ART_SCALE;              // 96 px
constexpr int ART_X    = (SCREEN_W - ART_W) / 2;            // centred
constexpr int BODY_Y   = ART_TOP_Y + ART_FLAME_ROWS * ART_SCALE;
constexpr int FLAME_CX = SCREEN_W / 2;
constexpr int FLAME_CY = ART_TOP_Y + 8 * ART_SCALE;

// ===========================================================================
//  Colour helpers
// ===========================================================================
inline uint16_t rgb(const Rgb& c) { return M5.Display.color565(c.r, c.g, c.b); }

inline Rgb lerpRgb(const Rgb& a, const Rgb& b, float t) {
    auto m = [&](uint8_t x, uint8_t y) {
        return (uint8_t)(x + (int)((y - x) * t));
    };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

inline float frand(float lo, float hi) {
    return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}

// ===========================================================================
//  Timer
// ===========================================================================
uint32_t remainingMs(uint32_t now) {
    if (g_state != State::Burning) return 0;
    const uint32_t elapsed = now - g_burn_start_ms;
    return (elapsed >= g_burn_budget_ms) ? 0 : (g_burn_budget_ms - elapsed);
}

// Segments left, from 0 to BAR_SEGMENTS (200).
int remainingSegments(uint32_t now) {
    const uint32_t rem = remainingMs(now);
    if (rem == 0) return 0;
    const uint32_t segs = (rem + MS_PER_SEGMENT - 1) / MS_PER_SEGMENT;  // round up
    return (int)(segs > BAR_SEGMENTS ? BAR_SEGMENTS : segs);
}

void lightTorch(uint32_t now) {
    uint32_t budget = TORCH_DURATION_MS;
    if (RESUME_AFTER_BLOWOUT && g_saved_remaining_ms > 0) {
        budget = g_saved_remaining_ms;  // resume the stowed torch
    }
    g_saved_remaining_ms = 0;
    g_burn_budget_ms     = budget;
    g_burn_start_ms      = now;
    g_state              = State::Burning;
    g_blow_locked        = BLOW_LOCKED_ON_LIGHT;  // every torch is born safe
    Serial.printf("[torch] lit, %lu ms\n", (unsigned long)budget);
}

void extinguish(uint32_t now, OutReason reason) {
    if (reason == OutReason::Blown && RESUME_AFTER_BLOWOUT) {
        g_saved_remaining_ms = remainingMs(now);
    } else {
        g_saved_remaining_ms = 0;
    }
    g_state            = State::Out;
    g_out_reason       = reason;
    g_out_since_ms     = now;
    g_lockout_until_ms = now + RELIGHT_LOCKOUT_MS;
    Serial.printf("[torch] out (%s)\n",
                  reason == OutReason::Blown ? "blown" : "burned out");
}

void updateTimer(uint32_t now) {
    if (g_state == State::Burning && remainingMs(now) == 0) {
        extinguish(now, OutReason::Expired);
    }
}

// ===========================================================================
//  Shake to light (accelerometer)
//  Several strong peaks are required within a window: a single knock on the
//  table produces one peak and lights nothing.
// ===========================================================================
uint8_t  g_shake_peaks    = 0;
uint32_t g_shake_first_ms = 0;
uint32_t g_shake_last_ms  = 0;
uint32_t g_imu_next_ms    = 0;

bool pollShake(uint32_t now) {
    if ((int32_t)(now - g_imu_next_ms) < 0) return false;
    g_imu_next_ms = now + IMU_POLL_MS;

    M5.Imu.update();
    const auto d = M5.Imu.getImuData();
    const float mag =
        sqrtf(d.accel.x * d.accel.x + d.accel.y * d.accel.y + d.accel.z * d.accel.z);
    const float dev = fabsf(mag - 1.0f);  // deviation from rest (1 g)

    // Expire the streak if it stalled halfway.
    if (g_shake_peaks > 0 && (now - g_shake_first_ms) > SHAKE_WINDOW_MS) {
        g_shake_peaks = 0;
    }

    if (dev > SHAKE_PEAK_G && (now - g_shake_last_ms) > SHAKE_PEAK_GAP_MS) {
        if (g_shake_peaks == 0) g_shake_first_ms = now;
        g_shake_last_ms = now;
        ++g_shake_peaks;
        if (g_shake_peaks >= SHAKE_PEAKS_NEEDED) {
            g_shake_peaks = 0;
            return true;
        }
    }
    return false;
}

// ===========================================================================
//  Blow to snuff (microphone)
//  Adaptive threshold: compared against the ambient noise measured live, and
//  the blow must be held a while (a clap or a shout will not do).
// ===========================================================================
int16_t  g_mic_buf[2][MIC_BLOCK_SAMPLES];
uint8_t  g_mic_idx    = 0;
bool     g_mic_primed = false;
float    g_noise_floor = 300.0f;
int32_t  g_blow_ms    = 0;

constexpr uint32_t MIC_BLOCK_MS = (MIC_BLOCK_SAMPLES * 1000) / MIC_SAMPLE_RATE;

void analyzeMicBlock(const int16_t* buf) {
    // Mean (removes the DC offset), then RMS.
    int64_t sum = 0;
    for (size_t i = 0; i < MIC_BLOCK_SAMPLES; ++i) sum += buf[i];
    const float mean = (float)sum / (float)MIC_BLOCK_SAMPLES;

    float acc = 0.0f;
    for (size_t i = 0; i < MIC_BLOCK_SAMPLES; ++i) {
        const float v = buf[i] - mean;
        acc += v * v;
    }
    const float rms = sqrtf(acc / (float)MIC_BLOCK_SAMPLES);

    // The noise floor tracks the room level, but a peak barely moves it: that
    // way a long blow does not "raise the bar" while you blow, and the floor
    // still climbs if the whole table gets noisy.
    const float alpha = (rms < g_noise_floor * 3.0f) ? 0.02f : 0.0015f;
    g_noise_floor += (rms - g_noise_floor) * alpha;
    if (g_noise_floor < 60.0f) g_noise_floor = 60.0f;

    const float threshold = fmaxf(BLOW_ABS_MIN_RMS, g_noise_floor * BLOW_FLOOR_RATIO);

    if (rms > threshold) {
        g_blow_ms += (int32_t)MIC_BLOCK_MS;
    } else {
        // Tolerates brief dips, but drains fast once you stop blowing.
        g_blow_ms -= (int32_t)MIC_BLOCK_MS * 2;
        if (g_blow_ms < 0) g_blow_ms = 0;
    }

#if MIC_DEBUG
    static uint32_t last_dbg = 0;
    if (millis() - last_dbg > 500) {
        last_dbg = millis();
        Serial.printf("[mic] rms=%.0f  noise=%.0f  thresh=%.0f  blow=%ld ms\n",
                      rms, g_noise_floor, threshold, (long)g_blow_ms);
    }
#endif
}

bool pollBlow() {
    if (!M5.Mic.isEnabled()) return false;

    // Double buffer: while one records, the previous one is analysed.
    if (M5.Mic.record(g_mic_buf[g_mic_idx], MIC_BLOCK_SAMPLES, MIC_SAMPLE_RATE)) {
        if (g_mic_primed) analyzeMicBlock(g_mic_buf[g_mic_idx ^ 1]);
        g_mic_primed = true;
        g_mic_idx ^= 1;
    }

    if (g_blow_ms >= (int32_t)BLOW_SUSTAIN_MS) {
        g_blow_ms = 0;
        return true;
    }
    return false;
}

// ===========================================================================
//  Battery (fuel gauge)
//  One dot per 5% of charge. Dots go out as soon as the charge drops, but only
//  come back once it is clearly past the edge, so they never blink.
// ===========================================================================
int      g_battery_dots    = -1;  // dots shown; -1 = no reading yet
uint32_t g_battery_next_ms = 0;

// Dots for a charge level, rounded up: 1% still shows one dot.
int batteryDotsFor(int level) {
    if (level <= 0) return 0;
    if (level >= 100) return BATTERY_DOTS;
    return (level * BATTERY_DOTS + 99) / 100;
}

void pollBattery(uint32_t now) {
    if ((int32_t)(now - g_battery_next_ms) < 0) return;
    g_battery_next_ms = now + BATTERY_POLL_MS;

    const int level = M5.Power.getBatteryLevel();
    if (level < 0) return;  // no reading: keep what is shown

    const int dots    = batteryDotsFor(level);
    const int clearly = batteryDotsFor(level - BATTERY_HYST_PCT);
    if (g_battery_dots < 0 || dots < g_battery_dots) {
        g_battery_dots = dots;     // first reading, or a dot spent
    } else if (clearly > g_battery_dots) {
        g_battery_dots = clearly;  // recharged well past the edge
    }
}

// ===========================================================================
//  Drawing
// ===========================================================================
void drawArtRows(const char* const* rows, int nrows, int y0, float dim) {
    for (int r = 0; r < nrows; ++r) {
        const char* row = rows[r];
        for (int c = 0; c < ART_COLS; ++c) {
            Rgb col;
            if (!artColor(row[c], col)) continue;
            const Rgb d = dimRgb(col, dim);
            g_gfx->fillRect(ART_X + c * ART_SCALE, y0 + r * ART_SCALE,
                            ART_SCALE, ART_SCALE, rgb(d));
        }
    }
}

// Warm halo behind the flame.
void drawGlow(float intensity) {
    struct Ring { int radius; Rgb color; };
    const Ring rings[] = {
        {104, { 12,  5, 1}},
        { 86, { 20,  8, 2}},
        { 70, { 31, 13, 3}},
        { 56, { 44, 18, 4}},
        { 42, { 58, 24, 5}},
    };
    for (const auto& ring : rings) {
        const Rgb c = dimRgb(ring.color, intensity);
        g_gfx->fillCircle(FLAME_CX, FLAME_CY, ring.radius, rgb(c));
    }
}

// Threads of smoke rising from the charred rag.
void drawSmoke(uint32_t frame) {
    for (int i = 0; i < 4; ++i) {
        const int phase = (int)((frame + i * 3) % 14);
        const int y     = ART_TOP_Y + 9 * ART_SCALE - phase * ART_SCALE;
        const int wob   = (int)(sinf((phase + i * 2) * 0.7f) * 2.0f);
        const float fade = 0.75f - phase * 0.055f;
        if (fade <= 0.05f) continue;
        Rgb c;
        artColor('S', c);
        g_gfx->fillRect(FLAME_CX + wob * ART_SCALE - ART_SCALE / 2, y,
                        ART_SCALE, ART_SCALE, rgb(dimRgb(c, fade)));
    }
}

void drawBar(int segments, bool lit) {
    const uint16_t frame_col = rgb({92, 78, 64});
    const uint16_t back_col  = rgb({20, 15, 12});

    g_gfx->fillRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, 4, frame_col);
    g_gfx->fillRoundRect(BAR_X + 2, BAR_Y + 2, BAR_W - 4, BAR_H - 4, 3, back_col);

    if (!lit || segments <= 0) return;

    const int inner_x = BAR_X + 3;
    const int inner_y = BAR_Y + 3;
    const int inner_w = BAR_W - 6;
    const int inner_h = BAR_H - 6;

    // Width quantised to the 200 segments.
    int fill_w = (inner_w * segments) / BAR_SEGMENTS;
    if (fill_w < 1) fill_w = 1;

    const float frac = (float)segments / (float)BAR_SEGMENTS;
    Rgb col = (frac > 0.5f)
                  ? lerpRgb({240, 124, 26}, {255, 196, 72}, (frac - 0.5f) * 2.0f)
                  : lerpRgb({186, 44, 16}, {240, 124, 26}, frac * 2.0f);

    // Last 5%: the bar pulses as a warning.
    if (segments <= BAR_SEGMENTS / 20 && ((g_frame_counter / 2) & 1)) {
        col = dimRgb(col, 0.45f);
    }

    g_gfx->fillRect(inner_x, inner_y, fill_w, inner_h, rgb(col));
    // Top highlight, gives the bar some volume.
    g_gfx->fillRect(inner_x, inner_y, fill_w, 3, rgb(dimRgb(col, 1.35f)));
}

void drawMessage(const char* line1, const char* line2, bool blink_line2) {
    g_gfx->setFont(&fonts::Font0);
    g_gfx->setTextDatum(lgfx::textdatum_t::middle_center);

    g_gfx->setTextSize(2);
    g_gfx->setTextColor(rgb({214, 186, 140}));
    g_gfx->drawString(line1, SCREEN_W / 2, 34);

    if (line2 && (!blink_line2 || ((g_frame_counter / 4) & 1))) {
        g_gfx->setTextSize(2);
        g_gfx->setTextColor(rgb({150, 116, 74}));
        g_gfx->drawString(line2, SCREEN_W / 2, 58);
    }
}

// Padlock below the bar: closed and dim = blowing does nothing; open and lit
// = the next blow snuffs it.
void drawLock(bool locked) {
    const char* const* art = locked ? LOCK_CLOSED : LOCK_OPEN;
    const uint16_t col = locked ? rgb({74, 66, 58}) : rgb({255, 190, 70});
    const int x0 = (SCREEN_W - LOCK_COLS * LOCK_SCALE) / 2;
    for (int r = 0; r < LOCK_ROWS; ++r) {
        for (int c = 0; c < LOCK_COLS; ++c) {
            if (art[r][c] != '#') continue;
            g_gfx->fillRect(x0 + c * LOCK_SCALE, LOCK_Y + r * LOCK_SCALE,
                            LOCK_SCALE, LOCK_SCALE, col);
        }
    }
}

// Battery dots on the padlock row, read left to right: they go out from the
// right, the same way the bar drains.
void drawBattery() {
    const uint16_t col  = rgb({84, 78, 72});
    const int      half = BATTERY_DOTS / 2;
    for (int i = 0; i < g_battery_dots; ++i) {
        const int x = (i < half)
            ? BAR_X + i * BATTERY_DOT_PITCH
            : SCREEN_W - BAR_X - BATTERY_DOT_SIZE -
                  (BATTERY_DOTS - 1 - i) * BATTERY_DOT_PITCH;
        g_gfx->fillRect(x, BATTERY_DOT_Y, BATTERY_DOT_SIZE, BATTERY_DOT_SIZE, col);
    }
}

void render(uint32_t now) {
    ++g_frame_counter;
    g_gfx->fillScreen(TFT_BLACK);

    if (g_state == State::Burning) {
        const int   segs = remainingSegments(now);
        const bool  dying = segs <= BAR_SEGMENTS / 10;  // last 10%

        // Flicker: the flame never looks the same two frames running.
        static int prev_frame = 0;
        int f = rand() % ART_FRAMES;
        if (f == prev_frame) f = (f + 1) % ART_FRAMES;
        if (dying && f == 1) f = 3;  // guttering, lower flame
        prev_frame = f;

        float dim = dying ? frand(0.55f, 0.80f) : frand(0.86f, 1.00f);

        drawGlow(dim * (dying ? 0.6f : 1.0f));
        drawArtRows(TORCH_FLAME[f], ART_FLAME_ROWS, ART_TOP_Y, dim);
        drawArtRows(TORCH_BODY, ART_BODY_ROWS, BODY_Y, 1.0f);
        drawBar(segs, true);
        drawLock(g_blow_locked);
    } else {
        drawArtRows(TORCH_HEAD_OUT, ART_FLAME_ROWS, ART_TOP_Y, 1.0f);
        drawArtRows(TORCH_BODY, ART_BODY_ROWS, BODY_Y, 0.85f);
        drawBar(0, false);

        if (g_state == State::Out) {
            if (now - g_out_since_ms < 20000) drawSmoke(g_frame_counter);
            if (g_out_reason == OutReason::Expired) {
                drawMessage("BURNED OUT", "SHAKE FOR A NEW TORCH", true);
            } else {
                drawMessage("SNUFFED", "SHAKE TO RELIGHT", true);
            }
        } else {
            drawMessage("SHADOWDARK", "SHAKE TO LIGHT", true);
        }
    }

    drawBattery();

    if (g_use_canvas) g_canvas.pushSprite(0, 0);
}

// ===========================================================================
//  Blow padlock (one tap arms, another protects)
// ===========================================================================
void pollTouch(uint32_t now) {
    const auto t = M5.Touch.getDetail();
    if (!t.wasPressed()) return;
    if (now - g_last_touch_ms < TOUCH_DEBOUNCE_MS) return;
    g_last_touch_ms = now;

    g_blow_locked = !g_blow_locked;
    // On arming, the blow counter restarts from zero: whatever the microphone
    // had piled up before does not count.
    g_blow_ms = 0;
    g_last_frame_ms = 0;  // repaint the padlock at once, without waiting a frame
    Serial.printf("[blow] %s\n", g_blow_locked ? "locked" : "armed");
}

}  // namespace

// ===========================================================================
//  setup / loop
// ===========================================================================
void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = true;   // BMI270: shake
    cfg.internal_mic = true;   // ES7210: blow
    cfg.internal_spk = false;  // on the CoreS3 mic and speaker share I2S
    cfg.clear_display = true;
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.setBrightness(SCREEN_BRIGHTNESS);
    M5.Display.fillScreen(TFT_BLACK);

    auto mic_cfg        = M5.Mic.config();
    mic_cfg.sample_rate = MIC_SAMPLE_RATE;
    M5.Mic.config(mic_cfg);
    M5.Mic.begin();

    // Off-screen canvas: the whole frame is composed and blitted in one go, so
    // the flame does not flicker while redrawing.
    g_canvas.setColorDepth(16);
    g_canvas.setPsram(true);
    if (g_canvas.createSprite(SCREEN_W, SCREEN_H)) {
        g_use_canvas = true;
        g_gfx        = &g_canvas;
    } else {
        g_use_canvas = false;
        g_gfx        = &M5.Display;  // out of memory: draw straight to the display
        Serial.println("[warn] no canvas, drawing directly");
    }

    srand(millis());
    pollBattery(millis());  // first reading, so the dots are there from boot

    Serial.println("[boot] Shadowdark Torch ready. Shake to light.");
    render(millis());
}

void loop() {
    M5.update();
    const uint32_t now = millis();

    pollTouch(now);
    pollBattery(now);

    updateTimer(now);

    const bool shaken = pollShake(now);
    const bool blown  = pollBlow();

    if ((int32_t)(now - g_lockout_until_ms) >= 0) {
        if (g_state == State::Burning) {
            if (blown && !g_blow_locked) extinguish(now, OutReason::Blown);
        } else if (shaken) {
            lightTorch(now);
        }
    }

    if (now - g_last_frame_ms >= FRAME_INTERVAL_MS) {
        g_last_frame_ms = now;
        render(now);
    }

    delay(2);
}
