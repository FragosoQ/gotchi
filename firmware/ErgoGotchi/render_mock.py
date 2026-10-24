#!/usr/bin/env python3
"""Mock do ecrã 368x448 com o novo layout gauge (simula ui_draw.h v2)."""
import math
from PIL import Image, ImageDraw, ImageFont
from gen_sprites import gen_all, PAL

W,H = 368,448
CX,CY = 184,212
R1,R2 = 176,158
A0,SWEEP,MAXV = 135,270,45
ANG_OK,ANG_WARN = 20,35

GREEN=(0x4A,0xDE,0x80); GOLD=(0xFF,0xCC,0x00); RED=(0xCC,0x33,0x00)
EVOG=(0xFF,0xD7,0x00); GRAY=(0x88,0x88,0x88); WHITE=(255,255,255)

def zone(frac):
    v=frac*MAXV
    return GREEN if v<ANG_OK else GOLD if v<ANG_WARN else RED

try: F3=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",30)
except: F3=ImageFont.load_default()
F2=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",20)

def center(d,txt,y,col,f):
    w=d.textbbox((0,0),txt,font=f)[2]
    d.text(((W-w)//2,y),txt,fill=col,font=f)

def screen(state_name, state_idx, hp, ang, lvl, streak, hora, batt, frame):
    img=Image.new("RGB",(W,H),(0,0,0)); d=ImageDraw.Draw(img)
    # gauge (segmentos 5 graus)
    for a in range(0,SWEEP,5):
        col=zone((a+2.5)/SWEEP)
        d.arc([CX-R1,CY-R1,CX+R1,CY+R1+0],A0+a,A0+a+5,fill=col,width=R1-R2)
    # marcador
    ma=A0+min(ang/MAXV,1)*SWEEP
    d.arc([CX-R1-5,CY-R1-5,CX+R1+5,CY+R1+5],ma-3,ma+3,fill=WHITE,width=R1-R2+10)
    # sprite
    fr=gen_all()
    key=["happy","warning","bad","evo"][state_idx]
    spr=fr[key][frame].convert("RGBA").resize((192,192),Image.NEAREST)
    mask=spr.split()[3] if spr.mode=="RGBA" else None
    # tornar preto transparente
    px=spr.load()
    for y in range(192):
        for x in range(192):
            r,g,b,a=px[x,y]
            if (r,g,b)==(0,0,0): px[x,y]=(0,0,0,0)
    img.paste(spr,(CX-96,CY-96),spr)
    # header
    d.text((10,6),hora,fill=WHITE,font=F2)
    d.text((CX-44,6),f"LVL {lvl}",fill=EVOG,font=F2)
    d.text((W-96,6),f"@ {batt}%",fill=WHITE,font=F2)
    # ângulo na abertura
    center(d,f"{ang:.0f}°",CY+118,GRAY,F2)
    # estado
    stcol=[GREEN,GOLD,RED,EVOG][state_idx]
    center(d,state_name,CY+R1-24,stcol,F3)
    # barra vitalidade
    bx,bw,bh,y = 44,W-88,16,CY+R1+12
    d.rounded_rectangle([bx,y,bx+bw,y+bh],8,outline=WHITE,width=2)
    hpcol=GREEN if hp>66 else GOLD if hp>33 else RED
    fw=int((bw-6)*hp/100)
    if fw>2: d.rounded_rectangle([bx+3,y+3,bx+3+fw,y+bh-3],5,fill=hpcol)
    center(d,f"VITALIDADE {hp}  EVO {streak}/30",y+bh+8,WHITE,F2)
    return img

shots=[
  screen("FELIZ",0,87,12,12,5,"16:13",84,0),
  screen("ATENCAO",1,55,27,12,5,"16:41",81,1),
  screen("CRITICO",2,22,41,12,0,"17:02",78,0),
  screen("SOLARION",3,98,8,24,30,"09:15",92,1),
]
pad=10
out=Image.new("RGB",(4*W+5*pad,H+2*pad),(24,24,32))
for i,s in enumerate(shots): out.paste(s,(pad+i*(W+pad),pad))
out.save("/home/claude/ergogotchi/screen_mock.png")
print("ok")
