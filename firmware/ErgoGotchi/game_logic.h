// ============================================================
// Ergo-Gotchi — game_logic.h (v3)
// HP/EXP/streak + FASES DE EVOLUÇÃO + MISSÃO DIÁRIA + HISTÓRICO
//
// Fases (dias de streak): Ember 0-29 | Solarion 30-89 |
//                         Guardian 90-179 | Phoenix 180+
// ============================================================
#pragma once
#include <Preferences.h>
#include <time.h>
#include "pin_config.h"

// Índices alinhados com SPRITES[] em sprites.h
enum GotchiState : uint8_t {
  ST_HAPPY = 0, ST_WARNING = 1, ST_BAD = 2,
  ST_SLEEP = 3, ST_CELEBRATE = 4,
  ST_SOLARION = 5, ST_GUARDIAN = 6, ST_PHOENIX = 7
};

class GotchiGame {
public:
  float    hp = 100.0f;
  uint32_t exp = 0;
  uint16_t streakDays = 0;
  GotchiState state = ST_HAPPY;

  uint32_t goodSecsToday = 0;      // tempo de boa postura hoje
  bool     missionDone = false;    // missão diária cumprida
  bool     eventCelebrate = false; // pedir animação de festa (missão/evolução)
  uint16_t hourGood[24] = {0};     // segundos bons por hora (histórico)
  uint16_t hourTotal[24] = {0};

  void begin(Preferences &prefs) {
    _prefs = &prefs;
    hp            = _prefs->getFloat ("hp", 100.0f);
    exp           = _prefs->getULong ("exp", 0);
    streakDays    = _prefs->getUShort("streak", 0);
    goodSecsToday = _prefs->getULong ("goodsecs", 0);
    missionDone   = _prefs->getBool  ("mission", false);
    _lastDay      = _prefs->getUShort("lastday", 0);
    _minHpToday   = hp;
  }

  // ---- Fase de evolução (0=Ember 1=Solarion 2=Guardian 3=Phoenix) ----
  uint8_t phase() const {
    if (streakDays >= 180) return 3;
    if (streakDays >= 90)  return 2;
    if (streakDays >= 30)  return 1;
    return 0;
  }
  const char* phaseName() const {
    static const char* n[4] = {"EMBER", "SOLARION", "GUARDIAN", "PHOENIX"};
    return n[phase()];
  }
  uint16_t nextPhaseDays() const {
    static const uint16_t t[4] = {30, 90, 180, 0};
    return t[phase()];
  }
  GotchiState happySprite() const {
    static const GotchiState s[4] = {ST_HAPPY, ST_SOLARION, ST_GUARDIAN, ST_PHOENIX};
    return s[phase()];
  }

  // ---- Tick 1 Hz. sleeping=true congela o jogo (colar pousado/noite) ----
  bool tick(float angle, bool sleeping) {
    GotchiState prev = state;
    if (sleeping) { state = ST_SLEEP; return false; }

    struct tm t;
    bool haveTime = getLocalTime(&t, 0);
    int hh = haveTime ? t.tm_hour : -1;

    if (angle < ANG_OK) {
      hp = min(hp + HP_GAIN_OK, 100.0f);
      exp += EXP_PER_SEC;
      goodSecsToday++;
      if (hh >= 0) hourGood[hh]++;
      state = happySprite();
      // missão diária cumprida?
      if (!missionDone && goodSecsToday >= MISSION_MIN * 60UL) {
        missionDone = true;
        exp += MISSION_EXP;
        hp = min(hp + 1.0f, 100.0f);
        eventCelebrate = true;
      }
    } else if (angle < ANG_WARN) {
      hp = max(hp - HP_LOSS_WARN, 0.0f);
      state = ST_WARNING;
    } else {
      hp = max(hp - HP_LOSS_BAD, 0.0f);
      state = ST_BAD;
    }
    if (hh >= 0) hourTotal[hh]++;

    _minHpToday = min(_minHpToday, hp);
    checkNewDay(haveTime ? t.tm_yday + 1 : 0);

    if (++_saveCounter >= 300) { save(); _saveCounter = 0; }
    return state != prev;
  }

  uint16_t level() const { return 1 + exp / 20000; }

  // Score ergonómico do dia (0-100)
  uint8_t scoreToday() const {
    uint32_t g = 0, tot = 0;
    for (int i = 0; i < 24; i++) { g += hourGood[i]; tot += hourTotal[i]; }
    return tot ? (uint8_t)(100UL * g / tot) : 100;
  }
  // Score de uma hora (0-100), 255 = sem dados
  uint8_t hourScore(int h) const {
    return hourTotal[h] ? (uint8_t)(100UL * hourGood[h] / hourTotal[h]) : 255;
  }

  const char* stateText() const {
    switch (state) {
      case ST_WARNING:   return "ATENCAO";
      case ST_BAD:       return "CRITICO";
      case ST_SLEEP:     return "A DORMIR";
      case ST_CELEBRATE: return "FESTA!";
      case ST_HAPPY:     return "FELIZ";
      default:           return phaseName();
    }
  }
  const char* postureText(float ang) const {
    if (ang < ANG_OK * 0.6f) return "Postura Excelente";
    if (ang < ANG_OK)        return "Postura Boa";
    if (ang < ANG_WARN)      return "Atencao a Postura";
    return "Postura Ma";
  }

  void save() {
    _prefs->putFloat ("hp", hp);
    _prefs->putULong ("exp", exp);
    _prefs->putUShort("streak", streakDays);
    _prefs->putULong ("goodsecs", goodSecsToday);
    _prefs->putBool  ("mission", missionDone);
    _prefs->putUShort("lastday", _lastDay);
  }

private:
  Preferences *_prefs = nullptr;
  float    _minHpToday = 100.0f;
  uint16_t _lastDay = 0;
  uint16_t _saveCounter = 0;

  void checkNewDay(uint16_t day) {
    if (day == 0) return;                 // sem relógio ainda
    if (_lastDay == 0) { _lastDay = day; return; }
    if (day != _lastDay) {
      uint8_t oldPhase = phase();
      if (_minHpToday >= STREAK_MIN_HP) streakDays++;
      else                              streakDays = 0;
      if (phase() != oldPhase) eventCelebrate = true;   // ⭐ evolução!
      _minHpToday = hp;
      _lastDay = day;
      goodSecsToday = 0;
      missionDone = false;
      memset(hourGood, 0, sizeof(hourGood));
      memset(hourTotal, 0, sizeof(hourTotal));
      save();
    }
  }
};
