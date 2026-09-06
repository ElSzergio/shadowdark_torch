# Shadowdark Torch — M5Stack CoreS3

A torch timer for **Shadowdark RPG**. In Shadowdark a torch lasts **one hour
of real time**: this gadget keeps track of it for you at the table, with no
phone timers and no counting in your head.

```
      /\        pixel-art torch, the flame animates at 4 fps
     /  \
    ( ** )
      ||
   [========------------]   bar = 200 segments, 85% of the width
```

## How you play it

| Action | Gesture |
|---|---|
| **Light** | Shake the CoreS3 like you mean it (several shakes in a row) |
| **Snuff** | Blow hard over the microphones on the side |
| **Burns down** | On its own, after 60 minutes |
| **Arm the blow** | A tap on the screen; another tap protects it again |

The bar **does not show minutes**, only how much torch is left: 200 segments,
one every 18 seconds. In the last 10% the flame shrinks and gutters; in the
last 5% the bar pulses. When it reaches zero the torch goes out and you have
to shake again to light a new one.

## Getting started

**What you need.** An **M5Stack CoreS3** and a USB-C cable that carries data
(a charge-only cable will not be seen by your computer). The project leans on
the CoreS3's touch screen, its BMI270 accelerometer and its ES7210
microphones; other M5Unified boards with an IMU and a mic may work by
changing `board` in [`platformio.ini`](platformio.ini), but the sensor
thresholds in `config.h` were measured on a real CoreS3, so expect to
recalibrate.

**Install PlatformIO.** Either the *PlatformIO IDE* extension for VS Code, or
the standalone CLI:

```bash
pip install -U platformio
```

**Build and flash.** Plug the CoreS3 in and, from the project folder:

```bash
pio run -t upload && pio device monitor
```

The first build downloads the ESP32 toolchain and M5Unified, so it takes a
few minutes; later builds take seconds. There is nothing else to configure —
the serial port is detected on its own, and the board's PSRAM, partitions and
USB-CDC come from the `m5stack-cores3` board definition.

**First run.** The screen shows an unlit torch and `SHAKE TO LIGHT`. Shake
the device firmly a few times and the flame catches; the bar starts draining,
one segment every 18 seconds. To put it out, **tap the screen first** — that
opens the padlock under the bar — and then blow hard on the microphone holes
until it goes out.

### If something goes wrong

| Symptom | What to do |
|---|---|
| `pio: command not found` | The VS Code extension installs it at `~/.platformio/penv/bin/pio` — add that to your `PATH` or call it by full path |
| Upload fails or no port is found | Try another USB-C cable first; if it still fails, put the CoreS3 into download mode (M5Stack's CoreS3 docs describe the button sequence) and upload again |
| Blowing does nothing | The padlock is closed: tap the screen to arm it. If it is already open, see [Calibrating the blow](#calibrating-the-blow) |
| It snuffs itself on any noise | Raise `BLOW_ABS_MIN_RMS` or `BLOW_FLOOR_RATIO` in [`src/config.h`](src/config.h) |
| Shaking will not light it | Lower `SHAKE_PEAK_G`, or drop `SHAKE_PEAKS_NEEDED` to 2 |

## Implementation notes

**Shake to light.** A single acceleration spike is not enough: it takes
`SHAKE_PEAKS_NEEDED` (3) peaks above `SHAKE_PEAK_G` (1.6 g over the resting
level), at least 70 ms apart, all inside a 1.5 s window. A knock on the table
or someone picking the device up produces a single peak and lights nothing.

**Blow to snuff.** The threshold is **adaptive**: the ambient noise is
measured live and the sound has to beat it by `BLOW_FLOOR_RATIO` (6×) for
`BLOW_SUSTAIN_MS` (640 ms) straight. That way it works the same at a quiet
table and in a bar, and you have to really blow: a long half-second rules out
claps, plosives and thumps on the table, which last a tenth of that.

**The blow padlock.** A freshly lit torch **cannot be blown out**: you have to
tap the screen to arm it. The padlock under the bar says so without words —
closed and dim means safe, open and amber means the next blow puts it out.

That is the defence against this gadget's real problem: the microphone cannot
tell a blow from any other loud, close noise, so instead of tuning thresholds
to exhaustion, the torch simply does not listen until you ask it to. Snuffing
becomes deliberate: tap, then blow.

The padlock closes itself again every time a new torch is lit, so it never
stays armed from one scene into the next.

**No flicker.** Every frame is composed whole on a canvas in PSRAM and
blitted in one go.

## Decisions made for you

They all live in [`src/config.h`](src/config.h) as constants; changing any of
them is a one-line edit:

| Decision | Value | Alternative |
|---|---|---|
| Every torch is **born protected**: you must tap before you can blow it out | `BLOW_LOCKED_ON_LIGHT = true` | `false` is born armed, and the tap is what protects it |
| Blowing **spends the torch entirely**: the next shake lights a new 60 min one | `RESUME_AFTER_BLOWOUT = false` | `true` stores the remaining time and resumes it on relighting |
| The on-screen labels are in English (`SHAKE TO LIGHT`, `BURNED OUT`) | — | `drawMessage(...)` in `src/main.cpp` |

The second row matters if at your table you snuff the torch to *stow* it:
with `RESUME_AFTER_BLOWOUT = true`, blowing stops burning torch.

## Calibrating the blow

The microphone level depends on your unit and on the room. If it is hard to
snuff, or it snuffs itself, set `#define MIC_DEBUG 1` in
[`src/main.cpp`](src/main.cpp), open the serial monitor and blow:

```
[mic] rms=412  noise=380  thresh=2280  blow=0 ms
[mic] rms=9840 noise=381  thresh=2286  blow=176 ms   <- blowing
```

Tune `BLOW_ABS_MIN_RMS` (absolute floor) and `BLOW_FLOOR_RATIO` in
`src/config.h` from those numbers. Same goes for `SHAKE_PEAK_G` if the shake
feels too stiff or too loose.

## Layout

```
src/config.h      every tunable parameter
src/torch_art.h   the pixel art (4 flame frames + body + snuffed)
src/main.cpp      state machine, sensors and drawing
```

The art is plain strings, one character per pixel: edited by hand, no tools
needed. The grid is 16 columns wide and each pixel is drawn at `ART_SCALE`
(7) screen pixels.
