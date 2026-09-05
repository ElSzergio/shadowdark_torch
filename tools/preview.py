"""Previsualiza en PNG lo que dibuja el CoreS3, leyendo src/torch_art.h y src/config.h."""
import re, math, sys
from PIL import Image, ImageDraw

import os
SRC = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src")
art = open(f"{SRC}/torch_art.h").read()

def block(name):
    """Devuelve la lista de listas de filas declaradas tras `name`."""
    i = art.index(name)
    j = art.index(";", i)
    rows = re.findall(r'"([^"]*)"', art[i:j])
    return rows

FLAME = block("TORCH_FLAME[ART_FRAMES]")
FRAMES = [FLAME[i*13:(i+1)*13] for i in range(4)]
HEAD_OUT = block("TORCH_HEAD_OUT[ART_FLAME_ROWS]")
BODY = block("TORCH_BODY[ART_BODY_ROWS]")

PAL = {}
for ch, r, g, b in re.findall(r"case '(.)': out = \{\s*(\d+),\s*(\d+),\s*(\d+)\}", art):
    PAL[ch] = (int(r), int(g), int(b))

cfg = open(f"{SRC}/config.h").read()
def cval(name):
    return int(re.search(rf"{name}\s*=\s*(\d+)", cfg).group(1))
W, H, S = cval("SCREEN_W"), cval("SCREEN_H"), cval("ART_SCALE")
COLS, FL_ROWS = 16, 13
ART_X = (W - COLS*S)//2
TOP_Y = cval("ART_TOP_Y"); BODY_Y = TOP_Y + FL_ROWS*S
BAR_W = int(W*0.85); BAR_X = (W-BAR_W)//2
BAR_Y, BAR_H = cval("BAR_Y"), cval("BAR_H")
main = open(f"{SRC}/main.cpp").read()
RINGS = [(int(r), (int(a), int(b), int(c))) for r, a, b, c in
         re.findall(r"\{\s*(\d+), \{\s*(\d+),\s*(\d+),\s*(\d+)\}\}", main)]
print("geometria:", dict(scale=S, top=TOP_Y, body=BODY_Y, bar=(BAR_X, BAR_Y, BAR_W, BAR_H)),
      "| fin arte:", BODY_Y + 13*S, "| anillos:", len(RINGS))

def dim(c, f):
    return tuple(max(0, min(255, int(v*f+0.5))) for v in c)

def draw_rows(d, rows, y0, f=1.0):
    for r, row in enumerate(rows):
        assert len(row) == COLS, f"fila de {len(row)} chars: {row!r}"
        for c, ch in enumerate(row):
            if ch not in PAL: continue
            x, y = ART_X + c*S, y0 + r*S
            d.rectangle([x, y, x+S-1, y+S-1], fill=dim(PAL[ch], f))

def draw_glow(d, f=1.0):
    cx, cy = W//2, TOP_Y + 8*S
    for rad, col in RINGS:
        d.ellipse([cx-rad, cy-rad, cx+rad, cy+rad], fill=dim(col, f))

def lerp(a, b, t): return tuple(int(x + (y-x)*t) for x, y in zip(a, b))

def draw_bar(d, segs, lit):
    d.rounded_rectangle([BAR_X, BAR_Y, BAR_X+BAR_W-1, BAR_Y+BAR_H-1], 4, fill=(92,78,64))
    d.rounded_rectangle([BAR_X+2, BAR_Y+2, BAR_X+BAR_W-3, BAR_Y+BAR_H-3], 3, fill=(20,15,12))
    if not lit or segs <= 0: return
    iw = BAR_W-6; fw = max(1, iw*segs//200)
    frac = segs/200
    col = lerp((240,124,26),(255,196,72),(frac-.5)*2) if frac > .5 else lerp((186,44,16),(240,124,26),frac*2)
    d.rectangle([BAR_X+3, BAR_Y+3, BAR_X+3+fw-1, BAR_Y+BAR_H-4], fill=col)
    d.rectangle([BAR_X+3, BAR_Y+3, BAR_X+3+fw-1, BAR_Y+5], fill=dim(col, 1.35))

def draw_text(img, txt, cy, col):
    """Font0 a tamano 2: rejilla de 6x8 escalada, 12 px por caracter."""
    from PIL import ImageFont
    f = ImageFont.load_default()
    tmp = Image.new("RGB", (len(txt)*6, 8), (0,0,0))
    ImageDraw.Draw(tmp).text((0,-2), txt, font=f, fill=col)
    tmp = tmp.resize((len(txt)*12, 16), Image.NEAREST)
    img.paste(tmp, ((img.width - tmp.width)//2, cy-8), tmp.convert("L").point(lambda v: 255 if v else 0))

def scene(frame, segs, lit, flicker=1.0):
    img = Image.new("RGB", (W, H), (0,0,0)); d = ImageDraw.Draw(img)
    if lit:
        draw_glow(d, flicker)
        draw_rows(d, FRAMES[frame], TOP_Y, flicker)
        draw_rows(d, BODY, BODY_Y, 1.0)
    else:
        draw_rows(d, HEAD_OUT, TOP_Y, 1.0)
        draw_rows(d, BODY, BODY_Y, .85)
    draw_bar(d, segs, lit)
    if not lit:
        draw_text(img, "SHADOWDARK", 34, (214,186,140))
        draw_text(img, "SHAKE TO LIGHT", 58, (150,116,74))
    return img

# Tira: 4 fotogramas ardiendo (barra llena -> agotandose) + apagada
shots = [scene(0, 200, True, 1.0), scene(1, 150, True, .92),
         scene(2, 96, True, .86), scene(3, 12, True, .65), scene(0, 0, False)]
strip = Image.new("RGB", (W*len(shots) + 8*(len(shots)-1), H), (40,40,40))
for i, s in enumerate(shots):
    strip.paste(s, (i*(W+8), 0))
strip = strip.resize((strip.width*2, strip.height*2), Image.NEAREST)
out = sys.argv[1] if len(sys.argv) > 1 else "preview.png"
strip.save(out)
print("frames:", len(FRAMES), "| paleta:", "".join(sorted(PAL)), "| ->", out)
