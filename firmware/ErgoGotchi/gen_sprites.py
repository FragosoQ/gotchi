#!/usr/bin/env python3
"""
Ergo-Gotchi — gerador de sprites 64x64, paleta 16 cores, estilo GBC.
Gera: sprites.h (4-bit packed), preview PNG e GIFs animados.
"""
import math
import numpy as np
from PIL import Image, ImageDraw

W = H = 64
CX = 32

# ---------------- Paleta (16 cores) ----------------
PAL = [
    (0x00, 0x00, 0x00),  # 0  transparente/fundo
    (0xFF, 0xF5, 0xE6),  # 1  núcleo branco
    (0xFF, 0xCC, 0x00),  # 2  brilho dourado
    (0xFF, 0x98, 0x00),  # 3  laranja médio
    (0xFF, 0x66, 0x00),  # 4  laranja escuro
    (0xCC, 0x33, 0x00),  # 5  vermelho sombra
    (0x99, 0x22, 0x00),  # 6  vermelho profundo
    (0x1A, 0x0A, 0x00),  # 7  outline
    (0xFF, 0xFF, 0xFF),  # 8  branco olhos
    (0x58, 0xA6, 0xFF),  # 9  azul suor
    (0x55, 0x55, 0x55),  # 10 cinza fumo
    (0x4A, 0xDE, 0x80),  # 11 verde HP
    (0xFF, 0xD7, 0x00),  # 12 dourado evo
    (0x88, 0x88, 0x88),  # 13 fumo claro
    (0x7A, 0x1A, 0x00),  # 14 vermelho muito escuro
    (0xFF, 0xE6, 0x80),  # 15 aura dourada pálida
]

def new_canvas():
    img = Image.new("P", (W, H), 0)
    flat = []
    for c in PAL:
        flat += list(c)
    img.putpalette(flat + [0, 0, 0] * (256 - 16))
    return img, ImageDraw.Draw(img)

def flame_body(d, top, base_y, half_w, wobble, layers, tip_dx=0):
    """Chama em camadas concêntricas (fora -> dentro)."""
    n = len(layers)
    for i, col in enumerate(layers):
        shrink = i / n
        hw = max(2, half_w * (1 - shrink * 0.72))
        t = top + (base_y - top) * shrink * 0.42
        b = base_y - shrink * 2.2
        # corpo: elipse inferior
        d.ellipse([CX - hw, b - hw * 1.15, CX + hw, b + hw * 0.55], fill=col)
        # tronco até à ponta (polígono ondulado)
        pts = []
        steps = 9
        for s in range(steps + 1):
            yy = b - (b - t) * (s / steps)
            frac = s / steps
            ww = hw * (1 - frac ** 1.35)
            off = wobble * math.sin(frac * 3.1 + i) * (frac ** 1.2) + tip_dx * frac
            pts.append((CX + off + ww, yy))
        for s in range(steps, -1, -1):
            yy = b - (b - t) * (s / steps)
            frac = s / steps
            ww = hw * (1 - frac ** 1.35)
            off = wobble * math.sin(frac * 3.1 + i) * (frac ** 1.2) + tip_dx * frac
            pts.append((CX + off - ww, yy))
        d.polygon(pts, fill=col)

def outline(img):
    """Contorno automático (índice 7) à volta do corpo."""
    a = np.array(img)
    body = a != 0
    out = a.copy()
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dx == 0 and dy == 0:
                continue
            sh = np.zeros_like(body)
            ys = slice(max(dy, 0), H + min(dy, 0))
            xs = slice(max(dx, 0), W + min(dx, 0))
            ys2 = slice(max(-dy, 0), H + min(-dy, 0))
            xs2 = slice(max(-dx, 0), W + min(-dx, 0))
            sh[ys2, xs2] = body[ys, xs]
            out[(sh) & (a == 0)] = 7
    img.putdata(out.flatten().tolist())
    return img

def eyes_happy(d, y, blink):
    for ex in (CX - 8, CX + 8):
        if blink:
            d.line([ex - 3, y, ex + 3, y], fill=7, width=2)
        else:
            d.ellipse([ex - 3, y - 4, ex + 3, y + 4], fill=8)
            d.ellipse([ex - 1, y - 1, ex + 1, y + 2], fill=7)

def eyes_worried(d, y):
    for ex, sl in ((CX - 8, 1), (CX + 8, -1)):
        d.ellipse([ex - 3, y - 3, ex + 3, y + 3], fill=8)
        d.ellipse([ex - 1, y, ex + 1, y + 2], fill=7)
        d.line([ex - 4, y - 5 - sl, ex + 4, y - 5 + sl], fill=7, width=2)  # sobrancelha

def eyes_dead(d, y):
    for ex in (CX - 8, CX + 8):
        d.line([ex - 3, y - 3, ex + 3, y + 3], fill=7, width=2)
        d.line([ex - 3, y + 3, ex + 3, y - 3], fill=7, width=2)

def mouth(d, y, kind):
    if kind == "smile":
        d.arc([CX - 5, y - 4, CX + 5, y + 3], 15, 165, fill=7, width=2)
    elif kind == "flat":
        d.line([CX - 4, y, CX + 4, y], fill=7, width=2)
    elif kind == "sad":
        d.arc([CX - 5, y, CX + 5, y + 7], 195, 345, fill=7, width=2)

def arms(d, y, up, col=4):
    """Bracinhos de chama."""
    for sx in (-1, 1):
        x0 = CX + sx * 17
        if up:
            d.line([x0, y, x0 + sx * 5, y - 7], fill=col, width=3)
            d.ellipse([x0 + sx * 5 - 2, y - 9, x0 + sx * 5 + 2, y - 5], fill=col)
        else:
            d.line([x0, y, x0 + sx * 4, y + 6], fill=col, width=3)

def sparks(d, frame, n, col=2):
    rng = np.random.RandomState(7 + frame)
    for _ in range(n):
        x = int(rng.uniform(10, 54))
        y = int(rng.uniform(2, 20))
        d.point([(x, y)], fill=col)
        if rng.rand() > 0.5:
            d.point([(x + 1, y)], fill=col)

def smoke(d, frame):
    ys = [8 - frame * 3, 16 - frame * 3, 24 - frame * 3]
    for i, y in enumerate(ys):
        y = y % 26 + 2
        x = CX - 4 + int(4 * math.sin(y * 0.5 + i))
        r = 2 + i
        col = 13 if i % 2 else 10
        d.ellipse([x - r, y - r, x + r, y + r], fill=col)

# ---------------- Frames por estado ----------------
def make_frame(state, frame, evolved=False):
    img, d = new_canvas()
    bounce = [0, -2, 0][frame]           # frame B salta
    wob = [2.0, -2.4, 1.2][frame]
    tip = [1, -3, 2][frame]

    if state == "happy":
        flame_body(d, top=5 + bounce, base_y=57 + bounce // 2, half_w=19,
                   wobble=wob, layers=[6, 5, 4, 3, 2], tip_dx=tip)
        # núcleo
        d.ellipse([CX - 8, 40 + bounce, CX + 8, 55 + bounce], fill=1)
        outline(img); d = ImageDraw.Draw(img)
        arms(d, 44 + bounce, up=True)
        eyes_happy(d, 36 + bounce, blink=(frame == 2))
        mouth(d, 45 + bounce, "smile")
        sparks(d, frame, 6)
        if evolved:
            # coroa
            for i, cx in enumerate((CX - 8, CX, CX + 8)):
                hy = 4 + bounce
                d.polygon([(cx - 3, hy + 6), (cx, hy - 2 - (i == 1) * 2), (cx + 3, hy + 6)], fill=12)
            d.rectangle([CX - 11, 9 + bounce, CX + 11, 11 + bounce], fill=12)
            # asas
            for sx in (-1, 1):
                x0 = CX + sx * 21
                d.polygon([(x0, 40 + bounce), (x0 + sx * 9, 28 + bounce),
                           (x0 + sx * 11, 40 + bounce), (x0 + sx * 6, 44 + bounce)], fill=12)
                d.polygon([(x0 + sx * 2, 42 + bounce), (x0 + sx * 8, 34 + bounce),
                           (x0 + sx * 9, 42 + bounce)], fill=15)
            # aura
            rng = np.random.RandomState(3 + frame)
            for _ in range(10):
                ang = rng.uniform(0, 6.28)
                r = rng.uniform(26, 30)
                x, y = CX + r * math.cos(ang), 36 + r * 0.9 * math.sin(ang)
                if 0 <= x < W and 0 <= y < H:
                    d.point([(x, y)], fill=15)

    elif state == "warning":
        flame_body(d, top=16 + bounce, base_y=58, half_w=17,
                   wobble=wob * 1.6, layers=[6, 5, 4, 3], tip_dx=tip * 1.5)
        d.ellipse([CX - 6, 44, CX + 6, 55], fill=2)
        outline(img); d = ImageDraw.Draw(img)
        arms(d, 47, up=False, col=5)
        eyes_worried(d, 40)
        mouth(d, 49, "flat")
        # gota de suor
        sy = 34 + frame
        d.ellipse([CX + 14, sy, CX + 18, sy + 6], fill=9)
        d.polygon([(CX + 14, sy + 2), (CX + 16, sy - 3), (CX + 18, sy + 2)], fill=9)

    else:  # bad / crítico
        flame_body(d, top=32 + bounce // 2, base_y=59, half_w=13,
                   wobble=wob * 0.7, layers=[14, 6, 5], tip_dx=tip)
        d.ellipse([CX - 4, 50, CX + 4, 57], fill=4)
        outline(img); d = ImageDraw.Draw(img)
        eyes_dead(d, 46)
        mouth(d, 53, "sad")
        smoke(d, frame)

    return img

STATES = [("happy", False), ("warning", False), ("bad", False), ("evo", True)]

def gen_all():
    frames = {}
    for name, evo in STATES:
        st = "happy" if evo else name
        frames[name] = [make_frame(st, f, evolved=evo) for f in range(3)]
    return frames

# ---------------- Export C header ----------------
def to_c(frames):
    def pack(img):
        a = np.array(img).astype(np.uint8)
        flat = a.flatten()
        packed = (flat[0::2] << 4) | flat[1::2]
        return packed

    def rgb565(c):
        r, g, b = c
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

    lines = []
    lines.append("// sprites.h — Ergo-Gotchi (gerado por gen_sprites.py)")
    lines.append("// 64x64 px, 4-bit packed (2 px/byte), paleta 16 cores RGB565")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("#define SPRITE_W 64")
    lines.append("#define SPRITE_H 64")
    lines.append("#define SPRITE_BYTES (SPRITE_W * SPRITE_H / 2)")
    lines.append("#define SPRITE_FRAMES 3")
    lines.append("")
    pal = ", ".join(f"0x{rgb565(c):04X}" for c in PAL)
    lines.append(f"static const uint16_t SPRITE_PALETTE[16] = {{ {pal} }};")
    lines.append("")
    for name, fr in frames.items():
        for i, img in enumerate(fr):
            p = pack(img)
            lines.append(f"static const uint8_t spr_{name}_{i}[SPRITE_BYTES] = {{")
            for off in range(0, len(p), 32):
                row = ", ".join(f"0x{b:02X}" for b in p[off:off + 32])
                lines.append(f"  {row},")
            lines.append("};")
            lines.append("")
    lines.append("// [estado][frame] — estados: 0=happy 1=warning 2=bad 3=evo(Solarion)")
    lines.append("static const uint8_t* const SPRITES[4][SPRITE_FRAMES] = {")
    for name, _ in STATES:
        lines.append("  { " + ", ".join(f"spr_{name}_{i}" for i in range(3)) + " },")
    lines.append("};")
    return "\n".join(lines)

def preview(frames, path):
    scale = 4
    pad = 8
    cols, rows = 3, 4
    img = Image.new("RGB", (cols * (W * scale + pad) + pad,
                            rows * (H * scale + pad) + pad), (24, 24, 32))
    for r, (name, _) in enumerate(STATES):
        for c in range(3):
            f = frames[name][c].convert("RGB").resize((W * scale, H * scale), Image.NEAREST)
            img.paste(f, (pad + c * (W * scale + pad), pad + r * (H * scale + pad)))
    img.save(path)

def gifs(frames, folder):
    for name, _ in STATES:
        fs = [f.convert("RGB").resize((W * 4, H * 4), Image.NEAREST) for f in frames[name]]
        fs[0].save(f"{folder}/gotchi_{name}.gif", save_all=True,
                   append_images=fs[1:], duration=333, loop=0)

if __name__ == "__main__":
    fr = gen_all()
    with open("/home/claude/ergogotchi/sprites.h", "w") as f:
        f.write(to_c(fr))
    preview(fr, "/home/claude/ergogotchi/sprites_preview.png")
    gifs(fr, "/home/claude/ergogotchi")
    import os
    print("sprites.h:", os.path.getsize("/home/claude/ergogotchi/sprites.h"), "bytes de texto")
    print("Flash por frame: 2048 B | 12 frames = 24 KB")
