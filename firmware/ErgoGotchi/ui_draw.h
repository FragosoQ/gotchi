// ============================================================
// Ergo-Gotchi — ui_draw.h (v3: 5 ecrãs com navegação por swipe)
//
//  HOME      sprite grande + fase + HP + postura + tempo de hoje
//  ANGULO    número grande + meio-gauge com marcador + qualidade
//  MISSAO    missão diária: progresso 30 min de boa postura
//  HISTORICO barras por hora (últimas 12h) + score do dia
//  CONFIG    Wi-Fi, brilho, vibração, som (toques)
// ============================================================
#pragma once
#include "Arduino_GFX_Library.h"
#include "sprites.h"
#include "game_logic.h"
#include "pin_config.h"

enum Screen : uint8_t { SCR_HOME = 0, SCR_ANGULO, SCR_MISSAO, SCR_HISTORICO, SCR_CONFIG, SCR_COUNT };

// Cores de UI (fora da paleta dos sprites)
#define UI_GRAY   0x8410
#define UI_DGRAY  0x39C7

struct UiSettings {
  uint8_t brightness = 220;
  uint8_t volume = 80;      // 0-100 (codec ES8311)
  bool vibra = true;
  bool som = true;
};

class GotchiUI {
public:
  GotchiUI(Arduino_GFX *gfx) : _gfx(gfx) {}
  UiSettings cfg;

  // ---------- infra ----------
  void clear() { _gfx->fillScreen(RGB565_BLACK); }

  void drawDots(Screen s) {
    int y = 434, cx0 = LCD_WIDTH / 2 - (SCR_COUNT - 1) * 12;
    for (int i = 0; i < SCR_COUNT; i++)
      _gfx->fillCircle(cx0 + i * 24, y, 5, i == s ? RGB565_WHITE : UI_DGRAY);
  }

  void drawHeader(int battPct, bool wifiOk) {
    _gfx->fillRect(0, 0, LCD_WIDTH, 30, RGB565_BLACK);
    _gfx->setTextSize(2);
    _gfx->setTextColor(RGB565_WHITE);
    _gfx->setCursor(10, 6);
    struct tm t;
    if (getLocalTime(&t, 0)) _gfx->printf("%02d:%02d", t.tm_hour, t.tm_min);
    else                     _gfx->print("--:--");
    _gfx->setCursor(LCD_WIDTH - 100, 6);
    _gfx->setTextColor(wifiOk ? SPRITE_PALETTE[11] : UI_DGRAY);
    _gfx->print("@");
    _gfx->setTextColor(RGB565_WHITE);
    if (battPct >= 0) _gfx->printf(" %d%%", battPct);
  }

  void centerText(const char *s, int y) {
    int16_t x1, y1; uint16_t w, h;
    _gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    _gfx->setCursor((LCD_WIDTH - w) / 2, y);
    _gfx->print(s);
  }

  uint16_t stateColor(GotchiState st) {
    switch (st) {
      case ST_BAD:     return SPRITE_PALETTE[5];
      case ST_WARNING: return SPRITE_PALETTE[2];
      case ST_SLEEP:   return SPRITE_PALETTE[14];
      case ST_HAPPY:   return SPRITE_PALETTE[11];
      default:         return SPRITE_PALETTE[12];   // fases/festa: dourado
    }
  }

  // ---------- sprite escalado (sc = 2 ou 3) ----------
  void drawSprite(GotchiState state, uint8_t frame, int x0, int y0, int sc) {
    const uint8_t *data = SPRITES[state][frame % SPRITE_FRAMES];
    static uint16_t lines[SPRITE_W * 3 * 3];
    for (int y = 0; y < SPRITE_H; y++) {
      for (int x = 0; x < SPRITE_W; x += 2) {
        uint8_t b   = data[(y * SPRITE_W + x) >> 1];
        uint16_t c1 = SPRITE_PALETTE[b >> 4];
        uint16_t c2 = SPRITE_PALETTE[b & 0x0F];
        int px = x * sc;
        for (int k = 0; k < sc; k++) {
          lines[px + k]      = c1;
          lines[px + sc + k] = c2;
        }
      }
      for (int k = 1; k < sc; k++)
        memcpy(&lines[k * SPRITE_W * sc], lines, SPRITE_W * sc * sizeof(uint16_t));
      _gfx->draw16bitRGBBitmap(x0, y0 + y * sc, lines, SPRITE_W * sc, sc);
    }
  }

  // ============================================================
  // ECRÃ 1 — HOME
  // ============================================================
  void drawHome(GotchiGame &g, float ang, int batt, bool wifi, uint8_t frame) {
    clear(); drawHeader(batt, wifi);
    drawSprite(g.state, frame, (LCD_WIDTH - 192) / 2, 46, 3);
    homeDynamics(g, ang, true);
    drawDots(SCR_HOME);
  }
  void homeDynamics(GotchiGame &g, float ang, bool full) {
    _gfx->fillRect(0, 244, LCD_WIDTH, 186, RGB565_BLACK);
    // nome/fase
    _gfx->setTextSize(3);
    _gfx->setTextColor(stateColor(g.state));
    centerText(g.stateText(), 248);
    // barra HP
    int bx = 44, bw = LCD_WIDTH - 88, bh = 16, y = 288;
    _gfx->drawRoundRect(bx, y, bw, bh, 8, RGB565_WHITE);
    uint16_t hpCol = g.hp > 66 ? SPRITE_PALETTE[11] : g.hp > 33 ? SPRITE_PALETTE[2] : SPRITE_PALETTE[5];
    int fw = (int)((bw - 6) * g.hp / 100.0f);
    if (fw > 2) _gfx->fillRoundRect(bx + 3, y + 3, fw, bh - 6, 5, hpCol);
    _gfx->setTextSize(2);
    _gfx->setTextColor(RGB565_WHITE);
    char b1[24]; snprintf(b1, sizeof(b1), "HP %d%%   LVL %u", (int)g.hp, g.level());
    centerText(b1, y + bh + 8);
    // postura + tempo de hoje
    _gfx->setTextColor(stateColor(g.state));
    centerText(g.postureText(ang), 348);
    _gfx->setTextColor(UI_GRAY);
    char b2[24];
    snprintf(b2, sizeof(b2), "Hoje %luh%02lu",
             (unsigned long)(g.goodSecsToday / 3600),
             (unsigned long)((g.goodSecsToday % 3600) / 60));
    centerText(b2, 378);
  }

  // ============================================================
  // ECRÃ 2 — ÂNGULO (meio-gauge)
  // ============================================================
  #define AG_CX (LCD_WIDTH/2)
  #define AG_CY 318
  #define AG_R1 130
  #define AG_R2 108
  void drawAngulo(GotchiGame &g, float ang, int batt, bool wifi) {
    clear(); drawHeader(batt, wifi);
    _gfx->setTextSize(3);
    _gfx->setTextColor(RGB565_WHITE);
    centerText("Angulo Cervical", 52);
    // escala 180°: 180->360 (semicírculo superior), 0..50° de valor
    for (int a = 0; a < 180; a += 5)
      _gfx->fillArc(AG_CX, AG_CY, AG_R1, AG_R2, 180 + a, 180 + a + 5,
                    zoneCol((a + 2.5f) / 180.0f * 50.0f));
    _markerA = -1000;
    anguloDynamics(g, ang);
    drawDots(SCR_ANGULO);
  }
  void anguloDynamics(GotchiGame &g, float ang) {
    // número grande
    _gfx->fillRect(0, 96, LCD_WIDTH, 88, RGB565_BLACK);
    _gfx->setTextSize(10);
    _gfx->setTextColor(zoneCol(ang));
    char nb[8]; snprintf(nb, sizeof(nb), "%2.0f", ang);
    centerText(nb, 100);
    // marcador no semicírculo
    float frac = constrain(ang / 50.0f, 0.0f, 1.0f);
    float a = 180 + frac * 180;
    if (fabsf(a - _markerA) >= 1.5f) {
      if (_markerA > -999) {
        _gfx->fillArc(AG_CX, AG_CY, AG_R1 + 6, AG_R2 - 6, _markerA - 5, _markerA + 5, RGB565_BLACK);
        float r0 = max(_markerA - 6, 180.0f), r1 = min(_markerA + 6, 359.9f);
        for (float s = r0; s < r1; s += 3)
          _gfx->fillArc(AG_CX, AG_CY, AG_R1, AG_R2, s, min(s + 3, r1),
                        zoneCol((s - 180) / 180.0f * 50.0f));
      }
      _gfx->fillArc(AG_CX, AG_CY, AG_R1 + 6, AG_R2 - 6, a - 3, a + 3, RGB565_WHITE);
      _markerA = a;
    }
    // qualidade
    _gfx->fillRect(0, 330, LCD_WIDTH, 60, RGB565_BLACK);
    _gfx->setTextSize(3);
    _gfx->setTextColor(zoneCol(ang));
    centerText(ang < ANG_OK * 0.6f ? "Muito Bom" :
               ang < ANG_OK        ? "Bom" :
               ang < ANG_WARN      ? "Atencao" : "Mau", 340);
  }

  // ============================================================
  // ECRÃ 3 — MISSÃO DIÁRIA
  // ============================================================
  void drawMissao(GotchiGame &g, uint8_t frame, int batt, bool wifi) {
    clear(); drawHeader(batt, wifi);
    _gfx->setTextSize(3); _gfx->setTextColor(RGB565_WHITE);
    centerText("Missao Diaria", 48);
    _gfx->setTextSize(2); _gfx->setTextColor(UI_GRAY);
    char t[32]; snprintf(t, sizeof(t), "Manter postura %d min", MISSION_MIN);
    centerText(t, 88);
    missaoDynamics(g);
    drawSprite(g.missionDone ? ST_CELEBRATE : g.happySprite(), frame,
               (LCD_WIDTH - 128) / 2, 250, 2);
    drawDots(SCR_MISSAO);
  }
  void missaoDynamics(GotchiGame &g) {
    int done = min((int)(g.goodSecsToday / 60), (int)MISSION_MIN);
    // 15 quadrados de progresso
    _gfx->fillRect(0, 122, LCD_WIDTH, 120, RGB565_BLACK);
    int n = 15, sq = 18, gap = 4;
    int x0 = (LCD_WIDTH - n * (sq + gap) + gap) / 2, y = 128;
    int fill = (int)((float)done / MISSION_MIN * n + 0.5f);
    for (int i = 0; i < n; i++)
      _gfx->fillRoundRect(x0 + i * (sq + gap), y, sq, sq, 3,
                          i < fill ? SPRITE_PALETTE[11] : UI_DGRAY);
    _gfx->setTextSize(4);
    _gfx->setTextColor(g.missionDone ? SPRITE_PALETTE[12] : RGB565_WHITE);
    char p[16]; snprintf(p, sizeof(p), "%d/%d", done, MISSION_MIN);
    centerText(p, 162);
    _gfx->setTextSize(2);
    if (g.missionDone) {
      _gfx->setTextColor(SPRITE_PALETTE[12]);
      char r[32]; snprintf(r, sizeof(r), "CUMPRIDA! +%d EXP", MISSION_EXP);
      centerText(r, 208);
    } else {
      _gfx->setTextColor(UI_GRAY);
      char r[32]; snprintf(r, sizeof(r), "Recompensa: +%d EXP", MISSION_EXP);
      centerText(r, 208);
    }
  }

  // ============================================================
  // ECRÃ 4 — HISTÓRICO (barras por hora)
  // ============================================================
  void drawHistorico(GotchiGame &g, int batt, bool wifi) {
    clear(); drawHeader(batt, wifi);
    _gfx->setTextSize(3); _gfx->setTextColor(RGB565_WHITE);
    centerText("Historico (Hoje)", 48);
    // últimas 12 horas
    struct tm t; int nowH = getLocalTime(&t, 0) ? t.tm_hour : 11;
    int n = 12, bw = 22, gap = 6;
    int x0 = (LCD_WIDTH - n * (bw + gap) + gap) / 2;
    int base = 320, maxH = 200;
    for (int i = 0; i < n; i++) {
      int h = (nowH - (n - 1) + i + 24) % 24;
      uint8_t sc = g.hourScore(h);
      int x = x0 + i * (bw + gap);
      if (sc == 255) {
        _gfx->fillRect(x, base - 8, bw, 8, UI_DGRAY);       // sem dados
      } else {
        int bh = max(8, sc * maxH / 100);
        uint16_t col = sc >= 80 ? SPRITE_PALETTE[11]
                     : sc >= 50 ? SPRITE_PALETTE[2] : SPRITE_PALETTE[5];
        _gfx->fillRect(x, base - bh, bw, bh, col);
      }
      if (i % 3 == 0) {
        _gfx->setTextSize(1); _gfx->setTextColor(UI_GRAY);
        _gfx->setCursor(x, base + 6);
        _gfx->printf("%02dh", h);
      }
    }
    // score do dia
    _gfx->drawRoundRect(84, 352, 200, 56, 10, RGB565_WHITE);
    _gfx->setTextSize(2); _gfx->setTextColor(UI_GRAY);
    _gfx->setCursor(100, 362); _gfx->print("Score");
    _gfx->setTextSize(3); _gfx->setTextColor(SPRITE_PALETTE[12]);
    _gfx->setCursor(180, 366); _gfx->printf("%d/100", g.scoreToday());
    drawDots(SCR_HISTORICO);
  }

  // ============================================================
  // ECRÃ 5 — CONFIG (linhas tocáveis)
  // ============================================================
  #define CFG_Y0 78
  #define CFG_ROW_H 70
  void drawConfig(bool wifi, int batt, const char *ip) {
    clear(); drawHeader(batt, wifi);
    _gfx->setTextSize(3); _gfx->setTextColor(RGB565_WHITE);
    centerText("Config", 44);
    configRow(0, "Wi-Fi", wifi ? ip : "---", wifi ? SPRITE_PALETTE[11] : UI_GRAY);
    configBrilho();
    configToggle(2, "Vibracao", cfg.vibra);
    configToggle(3, "Som", cfg.som);
    configVolume();
    drawDots(SCR_CONFIG);
  }
  void configRow(int i, const char *label, const char *val, uint16_t col) {
    int y = CFG_Y0 + i * CFG_ROW_H;
    _gfx->drawRoundRect(20, y, LCD_WIDTH - 40, CFG_ROW_H - 12, 10, UI_DGRAY);
    _gfx->setTextSize(2);
    _gfx->setTextColor(RGB565_WHITE);
    _gfx->setCursor(36, y + 20); _gfx->print(label);
    _gfx->setTextColor(col);
    // IPs são compridos: alinha à direita conforme o tamanho
    int w = strlen(val) * 12;
    _gfx->setCursor(LCD_WIDTH - 44 - w, y + 20); _gfx->print(val);
  }
  void configBrilho() {
    int i = 1, y = CFG_Y0 + i * CFG_ROW_H;
    _gfx->fillRect(21, y + 1, LCD_WIDTH - 42, CFG_ROW_H - 14, RGB565_BLACK);
    _gfx->drawRoundRect(20, y, LCD_WIDTH - 40, CFG_ROW_H - 12, 10, UI_DGRAY);
    _gfx->setTextSize(2); _gfx->setTextColor(RGB565_WHITE);
    _gfx->setCursor(36, y + 20); _gfx->print("Brilho");
    // [-]  barra  [+]
    _gfx->setCursor(150, y + 20); _gfx->print("-");
    _gfx->setCursor(LCD_WIDTH - 52, y + 20); _gfx->print("+");
    int bx = 172, bw = 120, bh = 12;
    _gfx->drawRoundRect(bx, y + 22, bw, bh, 6, RGB565_WHITE);
    int fw = (int)((bw - 6) * cfg.brightness / 255.0f);
    if (fw > 2) _gfx->fillRoundRect(bx + 3, y + 25, fw, bh - 6, 3, SPRITE_PALETTE[2]);
  }
  void configVolume() {
    int i = 4, y = CFG_Y0 + i * CFG_ROW_H;
    _gfx->fillRect(21, y + 1, LCD_WIDTH - 42, CFG_ROW_H - 14, RGB565_BLACK);
    _gfx->drawRoundRect(20, y, LCD_WIDTH - 40, CFG_ROW_H - 12, 10, UI_DGRAY);
    _gfx->setTextSize(2); _gfx->setTextColor(RGB565_WHITE);
    _gfx->setCursor(36, y + 20); _gfx->print("Volume");
    _gfx->setCursor(150, y + 20); _gfx->print("-");
    _gfx->setCursor(LCD_WIDTH - 52, y + 20); _gfx->print("+");
    int bx = 172, bw = 120, bh = 12;
    _gfx->drawRoundRect(bx, y + 22, bw, bh, 6, RGB565_WHITE);
    int fw = (int)((bw - 6) * cfg.volume / 100.0f);
    if (fw > 2) _gfx->fillRoundRect(bx + 3, y + 25, fw, bh - 6, 3, SPRITE_PALETTE[14]);
  }

  void configToggle(int i, const char *label, bool on) {
    int y = CFG_Y0 + i * CFG_ROW_H;
    _gfx->fillRect(21, y + 1, LCD_WIDTH - 42, CFG_ROW_H - 14, RGB565_BLACK);
    _gfx->drawRoundRect(20, y, LCD_WIDTH - 40, CFG_ROW_H - 12, 10, UI_DGRAY);
    _gfx->setTextSize(2); _gfx->setTextColor(RGB565_WHITE);
    _gfx->setCursor(36, y + 20); _gfx->print(label);
    // pílula on/off
    int px = LCD_WIDTH - 110, pw = 64, ph = 30, py = y + 14;
    _gfx->fillRoundRect(px, py, pw, ph, ph / 2, on ? SPRITE_PALETTE[11] : UI_DGRAY);
    _gfx->fillCircle(on ? px + pw - ph / 2 : px + ph / 2, py + ph / 2, ph / 2 - 4, RGB565_WHITE);
  }

  // toque no ecrã CONFIG -> devolve o que mudou:
  // 0 = nada, 1 = brilho, 2 = vibração, 3 = som, 4 = volume
  uint8_t configTap(int x, int y) {
    int row = (y - CFG_Y0) / CFG_ROW_H;
    if (row == 1) {                       // brilho
      if (x < LCD_WIDTH / 2) cfg.brightness = max(25, cfg.brightness - 25);
      else                   cfg.brightness = min(255, cfg.brightness + 25);
      configBrilho(); return 1;
    }
    if (row == 2) { cfg.vibra = !cfg.vibra; configToggle(2, "Vibracao", cfg.vibra); return 2; }
    if (row == 3) { cfg.som  = !cfg.som;  configToggle(3, "Som", cfg.som);       return 3; }
    if (row == 4) {                       // volume
      if (x < LCD_WIDTH / 2) cfg.volume = max(0, cfg.volume - 10);
      else                   cfg.volume = min(100, cfg.volume + 10);
      configVolume(); return 4;
    }
    return 0;
  }

private:
  Arduino_GFX *_gfx;
  float _markerA = -1000;

  uint16_t zoneCol(float val) {
    if (val < ANG_OK)   return SPRITE_PALETTE[11];
    if (val < ANG_WARN) return SPRITE_PALETTE[2];
    return SPRITE_PALETTE[5];
  }
};
