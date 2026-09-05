// ---------------------------------------------------------------------------
//  SHADOWDARK TORCH  —  M5Stack CoreS3
// ---------------------------------------------------------------------------
//  Una antorcha de Shadowdark dura 1 hora de tiempo real. Este proyecto la
//  lleva por ti:
//
//    * ENCENDER : sacude el CoreS3 con ganas (varios picos fuertes seguidos,
//                 un golpe a la mesa no basta).
//    * CONTADOR : barra de 200 fragmentos al 85% del ancho, 60 minutos.
//                 No se muestran los minutos, solo la barra.
//    * APAGAR   : sopla fuerte sobre los microfonos, o espera a que se agote.
//                 Al agotarse hay que repetir el proceso para encenderla.
//    * PANTALLA : un toque la pone en negro y congela la vista; otro toque
//                 vuelve a la vista anterior.
//
//  La vista se refresca cada 250 ms, animando la llama.
// ---------------------------------------------------------------------------

#include <M5Unified.h>
#include <esp_system.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "config.h"
#include "torch_art.h"

// Pon a 1 para volcar por serie el nivel del microfono y calibrar el soplido.
#define MIC_DEBUG 0

namespace {

// --- Estado ----------------------------------------------------------------
enum class State : uint8_t { Unlit, Burning, Out };
enum class OutReason : uint8_t { Expired, Blown };

State     g_state      = State::Unlit;
OutReason g_out_reason = OutReason::Expired;

uint32_t g_burn_start_ms      = 0;  // instante de encendido (ajustado por pausas)
uint32_t g_burn_budget_ms     = 0;  // duracion de esta antorcha
uint32_t g_saved_remaining_ms = 0;  // tiempo guardado al soplar (si se reanuda)
uint32_t g_out_since_ms       = 0;  // cuando se apago (para el humo)
uint32_t g_burned_ms          = 0;  // cuanto ardio, para el diagnostico
float    g_last_lf_ratio      = 0.0f;
const char* g_boot_reason     = "";
uint32_t g_lockout_until_ms   = 0;  // no leer sensores justo tras apagarse

// Pantalla en negro
bool     g_blackout            = false;
uint32_t g_blackout_started_ms = 0;
uint32_t g_last_touch_ms       = 0;

uint32_t g_last_frame_ms = 0;
uint32_t g_frame_counter = 0;

// --- Lienzo ----------------------------------------------------------------
M5Canvas   g_canvas(&M5.Display);
lgfx::LovyanGFX* g_gfx        = nullptr;
bool       g_use_canvas = false;

// --- Geometria derivada ----------------------------------------------------
constexpr int ART_W    = ART_COLS * ART_SCALE;              // 96 px
constexpr int ART_X    = (SCREEN_W - ART_W) / 2;            // centrada
constexpr int BODY_Y   = ART_TOP_Y + ART_FLAME_ROWS * ART_SCALE;
constexpr int FLAME_CX = SCREEN_W / 2;
constexpr int FLAME_CY = ART_TOP_Y + 8 * ART_SCALE;

// ===========================================================================
//  Utilidades de color
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
//  Contador
// ===========================================================================
uint32_t remainingMs(uint32_t now) {
    if (g_state != State::Burning) return 0;
    const uint32_t elapsed = now - g_burn_start_ms;
    return (elapsed >= g_burn_budget_ms) ? 0 : (g_burn_budget_ms - elapsed);
}

// Fragmentos que quedan, de 0 a BAR_SEGMENTS (200).
int remainingSegments(uint32_t now) {
    const uint32_t rem = remainingMs(now);
    if (rem == 0) return 0;
    const uint32_t segs = (rem + MS_PER_SEGMENT - 1) / MS_PER_SEGMENT;  // techo
    return (int)(segs > BAR_SEGMENTS ? BAR_SEGMENTS : segs);
}

void lightTorch(uint32_t now) {
    uint32_t budget = TORCH_DURATION_MS;
    if (RESUME_AFTER_BLOWOUT && g_saved_remaining_ms > 0) {
        budget = g_saved_remaining_ms;  // se reanuda la antorcha guardada
    }
    g_saved_remaining_ms = 0;
    g_burn_budget_ms     = budget;
    g_burn_start_ms      = now;
    g_state              = State::Burning;
    Serial.printf("[torch] encendida, %lu ms\n", (unsigned long)budget);
}

void extinguish(uint32_t now, OutReason reason) {
    if (reason == OutReason::Blown && RESUME_AFTER_BLOWOUT) {
        g_saved_remaining_ms = remainingMs(now);
    } else {
        g_saved_remaining_ms = 0;
    }
    g_burned_ms        = now - g_burn_start_ms;
    g_state            = State::Out;
    g_out_reason       = reason;
    g_out_since_ms     = now;
    g_lockout_until_ms = now + RELIGHT_LOCKOUT_MS;
    Serial.printf("[torch] apagada (%s) tras %lu s ardiendo; graves=%.2f\n",
                  reason == OutReason::Blown ? "soplido" : "agotada",
                  (unsigned long)(g_burned_ms / 1000), g_last_lf_ratio);
}

void updateTimer(uint32_t now) {
    if (g_state == State::Burning && remainingMs(now) == 0) {
        extinguish(now, OutReason::Expired);
    }
}

// ===========================================================================
//  Encendido por sacudida (acelerometro)
//  Se exigen varios picos fuertes dentro de una ventana: un golpe suelto a la
//  mesa produce un unico pico y no enciende nada.
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
    const float dev = fabsf(mag - 1.0f);  // desviacion respecto al reposo (1 g)

    // Caduca la racha si se ha quedado a medias.
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
//  Apagado soplando (microfono)
//  Umbral adaptativo: se compara con el ruido ambiente medido en vivo, y hay
//  que mantener el soplido un rato (una palmada o un grito no bastan).
// ===========================================================================
int16_t  g_mic_buf[2][MIC_BLOCK_SAMPLES];
uint8_t  g_mic_idx    = 0;
bool     g_mic_primed = false;
float    g_noise_floor = 300.0f;
int32_t  g_blow_ms    = 0;

constexpr uint32_t MIC_BLOCK_MS = (MIC_BLOCK_SAMPLES * 1000) / MIC_SAMPLE_RATE;

void analyzeMicBlock(const int16_t* buf) {
    // Media, para quitar la continua del bloque.
    int64_t sum = 0;
    for (size_t i = 0; i < MIC_BLOCK_SAMPLES; ++i) sum += buf[i];
    const float mean = (float)sum / (float)MIC_BLOCK_SAMPLES;

    // Energia total y energia grave, en la misma pasada. El paso bajo son dos
    // polos en cascada a ~120 Hz; su estado sigue vivo entre bloques.
    static float lp1 = 0.0f, lp2 = 0.0f;
    float acc = 0.0f, acc_lf = 0.0f;
    for (size_t i = 0; i < MIC_BLOCK_SAMPLES; ++i) {
        const float v = buf[i] - mean;
        lp1 += (v - lp1) * MIC_LP_ALPHA;
        lp2 += (lp1 - lp2) * MIC_LP_ALPHA;
        acc += v * v;
        acc_lf += lp2 * lp2;
    }
    const float rms    = sqrtf(acc / (float)MIC_BLOCK_SAMPLES);
    const float rms_lf = sqrtf(acc_lf / (float)MIC_BLOCK_SAMPLES);
    // Que fraccion del sonido es grave: soplar >1, hablar 0.1-0.25.
    const float lf_ratio = (rms > 1.0f) ? (rms_lf / rms) : 0.0f;
    g_last_lf_ratio = lf_ratio;

    // Nivel alisado: el soplido es tan grave que en 16 ms cabe apenas un ciclo
    // y el valor eficaz da bandazos. Sin esto el soplido no se sostiene.
    static float level = 0.0f;
    level += (rms - level) * MIC_LEVEL_SMOOTH;

    // El ruido ambiente sigue al nivel de la sala, pero un pico apenas lo
    // mueve: asi un soplido largo no "sube el liston" mientras soplas, y aun
    // asi el suelo acaba subiendo si la mesa entera se pone ruidosa.
    const float alpha = (rms < g_noise_floor * 3.0f) ? 0.02f : 0.0015f;
    g_noise_floor += (rms - g_noise_floor) * alpha;
    if (g_noise_floor < 60.0f) g_noise_floor = 60.0f;

    const float threshold = fmaxf(BLOW_ABS_MIN_RMS, g_noise_floor * BLOW_FLOOR_RATIO);

    if (level > threshold && lf_ratio > BLOW_LF_RATIO_MIN) {
        g_blow_ms += (int32_t)MIC_BLOCK_MS;
    } else {
        // Tolera bajones breves, pero se vacia rapido al dejar de soplar.
        g_blow_ms -= (int32_t)(MIC_BLOCK_MS * BLOW_DECAY_MULT);
        if (g_blow_ms < 0) g_blow_ms = 0;
    }

#if MIC_DEBUG
    static uint32_t last_dbg = 0;
    if (millis() - last_dbg > 500) {
        last_dbg = millis();
        Serial.printf("[mic] nivel=%.0f (%.1f%% FS)  graves=%.2f  ruido=%.0f  "
                      "umbral=%.0f  soplido=%ld ms\n",
                      level, level * 100.0f / 32767.0f, lf_ratio, g_noise_floor,
                      threshold, (long)g_blow_ms);
    }
#endif
}

bool pollBlow() {
    if (!M5.Mic.isEnabled()) return false;

    // Doble bufer: mientras uno se graba se analiza el anterior.
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
//  Dibujo
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

// Halo calido detras de la llama.
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

// Hilos de humo subiendo desde el trapo carbonizado.
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

    // Ancho cuantizado a los 200 fragmentos.
    int fill_w = (inner_w * segments) / BAR_SEGMENTS;
    if (fill_w < 1) fill_w = 1;

    const float frac = (float)segments / (float)BAR_SEGMENTS;
    Rgb col = (frac > 0.5f)
                  ? lerpRgb({240, 124, 26}, {255, 196, 72}, (frac - 0.5f) * 2.0f)
                  : lerpRgb({186, 44, 16}, {240, 124, 26}, frac * 2.0f);

    // Ultimo 5%: la barra late para avisar.
    if (segments <= BAR_SEGMENTS / 20 && ((g_frame_counter / 2) & 1)) {
        col = dimRgb(col, 0.45f);
    }

    g_gfx->fillRect(inner_x, inner_y, fill_w, inner_h, rgb(col));
    // Brillo superior, da volumen a la barra.
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

// Linea pequena bajo la barra: por que se apago, o como arranco el aparato.
void drawFootnote(const char* txt) {
    g_gfx->setFont(&fonts::Font0);
    g_gfx->setTextSize(1);
    g_gfx->setTextDatum(lgfx::textdatum_t::middle_center);
    g_gfx->setTextColor(rgb({96, 84, 70}));
    g_gfx->drawString(txt, SCREEN_W / 2, SCREEN_H - 11);
}

void render(uint32_t now) {
    ++g_frame_counter;
    g_gfx->fillScreen(TFT_BLACK);

    if (g_state == State::Burning) {
        const int   segs = remainingSegments(now);
        const bool  dying = segs <= BAR_SEGMENTS / 10;  // ultimo 10%

        // Parpadeo: la llama nunca se ve dos fotogramas igual.
        static int prev_frame = 0;
        int f = rand() % ART_FRAMES;
        if (f == prev_frame) f = (f + 1) % ART_FRAMES;
        if (dying && f == 1) f = 3;  // agonizando, llama mas baja
        prev_frame = f;

        float dim = dying ? frand(0.55f, 0.80f) : frand(0.86f, 1.00f);

        drawGlow(dim * (dying ? 0.6f : 1.0f));
        drawArtRows(TORCH_FLAME[f], ART_FLAME_ROWS, ART_TOP_Y, dim);
        drawArtRows(TORCH_BODY, ART_BODY_ROWS, BODY_Y, 1.0f);
        drawBar(segs, true);
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
            if (SHOW_OUT_DEBUG) {
                const uint32_t secs = g_burned_ms / 1000;
                char note[40];
                snprintf(note, sizeof(note), "OUT AT %lu:%02lu - %s",
                         (unsigned long)(secs / 60), (unsigned long)(secs % 60),
                         g_out_reason == OutReason::Expired ? "BURNED" : "SNUFFED");
                drawFootnote(note);
            }
        } else {
            drawMessage("SHADOWDARK", "SHAKE TO LIGHT", true);
            if (SHOW_OUT_DEBUG) drawFootnote(g_boot_reason);
        }
    }

    if (g_use_canvas) g_canvas.pushSprite(0, 0);
}

// ===========================================================================
//  Pantalla en negro (un toque apaga, otro reanuda)
// ===========================================================================
void enterBlackout(uint32_t now) {
    g_blackout            = true;
    g_blackout_started_ms = now;
    M5.Display.fillScreen(TFT_BLACK);
    if (BLACKOUT_TURNS_OFF_BACKLIGHT) M5.Display.setBrightness(0);
    Serial.println("[screen] negro");
}

void exitBlackout(uint32_t now) {
    g_blackout = false;
    if (BLACKOUT_PAUSES_TIMER && g_state == State::Burning) {
        g_burn_start_ms += (now - g_blackout_started_ms);  // el tiempo no conto
    }
    g_blow_ms     = 0;  // no arrastres lecturas viejas del microfono
    g_mic_primed  = false;
    g_shake_peaks = 0;
    if (BLACKOUT_TURNS_OFF_BACKLIGHT) M5.Display.setBrightness(SCREEN_BRIGHTNESS);
    g_last_frame_ms = 0;  // fuerza un repintado inmediato
    Serial.println("[screen] vista reanudada");
}

void pollTouch(uint32_t now) {
    const auto t = M5.Touch.getDetail();
    if (!t.wasPressed()) return;
    if (now - g_last_touch_ms < TOUCH_DEBOUNCE_MS) return;
    g_last_touch_ms = now;
    g_blackout ? exitBlackout(now) : enterBlackout(now);
}

// ===========================================================================
//  setup / loop
// ===========================================================================
// Si la antorcha "se apaga sola" y al mirar pone SHAKE TO LIGHT, es que el
// aparato se reinicio: esto dice por que.
const char* bootReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:  return "BOOT: POWER ON";
        case ESP_RST_SW:       return "BOOT: SOFT RESET";
        case ESP_RST_PANIC:    return "BOOT: CRASH";
        case ESP_RST_INT_WDT:  return "BOOT: INT WATCHDOG";
        case ESP_RST_TASK_WDT: return "BOOT: TASK WATCHDOG";
        case ESP_RST_WDT:      return "BOOT: WATCHDOG";
        case ESP_RST_BROWNOUT: return "BOOT: BROWNOUT";
        case ESP_RST_DEEPSLEEP:return "BOOT: DEEP SLEEP";
        default:               return "BOOT: UNKNOWN";
    }
}

}  // namespace

void setup() {
    g_boot_reason = bootReason();
    Serial.printf("[boot] %s\n", g_boot_reason);

    auto cfg = M5.config();
    cfg.internal_imu = true;   // BMI270: sacudida
    cfg.internal_mic = true;   // ES7210: soplido
    cfg.internal_spk = false;  // en el CoreS3 microfono y altavoz comparten I2S
    cfg.clear_display = true;
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.setBrightness(SCREEN_BRIGHTNESS);
    M5.Display.fillScreen(TFT_BLACK);

    auto mic_cfg        = M5.Mic.config();
    mic_cfg.sample_rate = MIC_SAMPLE_RATE;
    M5.Mic.config(mic_cfg);
    M5.Mic.begin();

    // Lienzo fuera de pantalla: se compone el cuadro entero y se vuelca de
    // golpe, asi la llama no parpadea al redibujar.
    g_canvas.setColorDepth(16);
    g_canvas.setPsram(true);
    if (g_canvas.createSprite(SCREEN_W, SCREEN_H)) {
        g_use_canvas = true;
        g_gfx        = &g_canvas;
    } else {
        g_use_canvas = false;
        g_gfx        = &M5.Display;  // sin memoria: dibuja directo
        Serial.println("[warn] sin lienzo, se dibuja directo");
    }

    srand(millis());

    Serial.println("[boot] Shadowdark Torch listo. Sacude para encender.");
    render(millis());
}

void loop() {
    M5.update();
    const uint32_t now = millis();

    pollTouch(now);

    if (g_blackout) {
        // Pantalla en negro: no se dibuja ni se leen sensores. El contador
        // sigue corriendo salvo que BLACKOUT_PAUSES_TIMER lo pida.
        if (!BLACKOUT_PAUSES_TIMER) updateTimer(now);
        delay(5);
        return;
    }

    updateTimer(now);

    const bool shaken = pollShake(now);
    const bool blown  = pollBlow();

    if ((int32_t)(now - g_lockout_until_ms) >= 0) {
        if (g_state == State::Burning) {
            if (blown) extinguish(now, OutReason::Blown);
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
