"""Reproduce en el ordenador el detector de soplido de main.cpp, con los
parametros reales de config.h, y lo enfrenta a senales sinteticas."""
import math, random, re

import os
SRC = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src")
cfg = open(f"{SRC}/config.h").read()
def cval(n):
    m = re.search(rf"{n}\s*=\s*([0-9.]+)f?", cfg)
    return float(m.group(1))

FS      = int(cval("MIC_SAMPLE_RATE"))
BLOCK   = int(cval("MIC_BLOCK_SAMPLES"))
ALPHA   = cval("MIC_LP_ALPHA")
LF_MIN  = cval("BLOW_LF_RATIO_MIN")
PCT     = cval("BLOW_MIN_LEVEL_PCT")
RATIO   = cval("BLOW_FLOOR_RATIO")
SUSTAIN = cval("BLOW_SUSTAIN_MS")
SMOOTH  = cval("MIC_LEVEL_SMOOTH")
DECAY   = cval("BLOW_DECAY_MULT")
ABS_MIN = 32767.0 * PCT / 100.0
BLOCK_MS = BLOCK * 1000 // FS
FS_MAX = 32767.0
print(f"config: alisado={SMOOTH} decaimiento={DECAY}x fs={FS} bloque={BLOCK} ({BLOCK_MS} ms) alpha={ALPHA} "
      f"graves>{LF_MIN} nivel>{ABS_MIN:.0f} ({PCT}% FS) sostener>{SUSTAIN:.0f} ms\n")

class Detector:
    """Copia fiel de analyzeMicBlock() + pollBlow()."""
    def __init__(self, floor=300.0):
        self.lp1 = self.lp2 = 0.0
        self.level = 0.0
        self.floor = floor
        self.blow_ms = 0
        self.fired = False
        self.peak_lf = 0.0
        self.peak_rms = 0.0
    def block(self, samples):
        mean = sum(samples) / len(samples)
        acc = acc_lf = 0.0
        for x in samples:
            v = x - mean
            self.lp1 += (v - self.lp1) * ALPHA
            self.lp2 += (self.lp1 - self.lp2) * ALPHA
            acc += v*v; acc_lf += self.lp2*self.lp2
        rms    = math.sqrt(acc / len(samples))
        rms_lf = math.sqrt(acc_lf / len(samples))
        lf = rms_lf / rms if rms > 1.0 else 0.0
        a = 0.02 if rms < self.floor*3 else 0.0015
        self.floor += (rms - self.floor) * a
        self.floor = max(self.floor, 60.0)
        self.level += (rms - self.level) * SMOOTH
        thr = max(ABS_MIN, self.floor * RATIO)
        if self.level > thr and lf > LF_MIN: self.blow_ms += BLOCK_MS
        else: self.blow_ms = max(0, self.blow_ms - int(BLOCK_MS*DECAY))
        if self.blow_ms >= SUSTAIN: self.fired = True
        self.peak_lf = max(self.peak_lf, lf); self.peak_rms = max(self.peak_rms, self.level)
        return rms, lf

def lowpassed_noise(n, fc, amp):
    """Ruido pasado por dos polos: turbulencia de aire."""
    a = 1 - math.exp(-2*math.pi*fc/FS)
    y1 = y2 = 0.0; out = []
    for _ in range(n):
        x = random.uniform(-1, 1)
        y1 += (x - y1)*a; y2 += (y1 - y2)*a
        out.append(y2)
    peak = max(abs(v) for v in out) or 1.0
    return [v/peak*amp for v in out]

def voice(n, f0, amp, breathy=0.15):
    """Vocal: armonicos de f0 con formantes en 700/1200/2600 Hz + soplo."""
    out = []
    for i in range(n):
        t = i/FS
        env = 0.75 + 0.25*math.sin(2*math.pi*4*t)      # modulacion silabica
        s = 0.0
        for k in range(1, 30):
            f = f0*k
            if f > FS/2: break
            g = 1.0/k                                   # caida natural
            for fc, q in ((700,1.9), (1200,1.5), (2600,1.0)):
                g += q/(1 + ((f-fc)/120.0)**2)          # formantes
            s += g*math.sin(2*math.pi*f*t + k)
        s = s/12.0 + breathy*random.uniform(-1,1)
        out.append(max(-1, min(1, s*env)))
    peak = max(abs(v) for v in out) or 1.0
    return [v/peak*amp for v in out]

def run(name, sig, floor=300.0):
    d = Detector(floor)
    for i in range(0, len(sig) - BLOCK, BLOCK):
        d.block([v*FS_MAX for v in sig[i:i+BLOCK]])
    verdict = "APAGA  <-- " if d.fired else "no apaga"
    print(f"  {name:<34} rms_max={d.peak_rms:6.0f} ({d.peak_rms/FS_MAX*100:4.1f}% FS)"
          f"  graves_max={d.peak_lf:.2f}  {verdict}")
    return d.fired

SEC = FS
random.seed(7)
print("SOPLIDOS (deben apagar):")
ok  = run("soplar fuerte 1.5 s",          lowpassed_noise(int(1.5*SEC), 70, 0.45))
ok2 = run("soplar con ganas 1 s",         lowpassed_noise(1*SEC, 80, 0.32))
print("\nSOPLIDO FLOJO (da igual: se pidio soplar fuerte):")
run("soplido flojo 1 s",                  lowpassed_noise(1*SEC, 90, 0.22))
print("\nRUIDO DE MESA (no deben apagar):")
bad = []
bad.append(run("hablar cerca, 3 s",              voice(3*SEC, 120, 0.06)))
bad.append(run("hablar fuerte a un palmo, 3 s",  voice(3*SEC, 120, 0.20)))
bad.append(run("voz grave gritando, 2 s",        voice(2*SEC, 95,  0.35)))
bad.append(run("voz aguda gritando, 2 s",        voice(2*SEC, 200, 0.35)))
bad.append(run("palmada (60 ms)",  lowpassed_noise(int(0.06*SEC), 70, 0.9) + [0.0]*SEC))
bad.append(run("plosivas 'p' seguidas",
               sum(([*lowpassed_noise(int(0.05*SEC), 60, 0.7)] + [0.0]*int(0.2*SEC)
                    for _ in range(6)), [])))
bad.append(run("golpe en la mesa (LF fuerte, 120 ms)",
               lowpassed_noise(int(0.12*SEC), 40, 1.0) + [0.0]*SEC))
bad.append(run("bar ruidoso de fondo, 4 s", [0.05*random.uniform(-1,1) for _ in range(4*SEC)]))

print("\n" + ("RESULTADO: correcto" if (ok and ok2 and not any(bad))
              else "RESULTADO: REVISAR umbrales"))
