// ============================================================
// Ergo-Gotchi — game_logic.h (v4: morte e ressurreição 👻)
//
// Morte: HP a 0 + má postura durante DEATH_SECS -> vira fantasma
//        (streak a zero, EXP a metade). Boa postura zera o contador.
// Ressurreição: RES_SECS acumulados de boa postura -> renasce (HP 50).
// ============================================================
#pragma once
#include <Preferences.h>
#include <time.h>
#include "pin_config.h"
#include "lang.h"

// Índices alinhados com SPRITES[] em sprites.h
enum GotchiState : uint8_t {
  ST_HAPPY = 0, ST_WARNING = 1, ST_BAD = 2,
  ST_SLEEP = 3, ST_CELEBRATE = 4,
  ST_SOLARION = 5, ST_GUARDIAN = 6, ST_PHOENIX = 7,
  ST_GHOST = 8
};

class GotchiGame {
public:
  float    hp = 100.0f;
  uint32_t exp = 0;
  uint16_t streakDays = 0;
  GotchiState state = ST_HAPPY;

  bool     dead = false;           // 👻 modo fantasma
  uint32_t deadRes = 0;            // segs bons acumulados p/ ressuscitar
  uint16_t deaths = 0;
  uint32_t hp0Secs = 0;            // segs seguidos com HP=0 em má postura

  uint32_t goodSecsToday = 0;
  bool     missionDone = false;
  bool     eventCelebrate = false; // festa (missão/evolução/ressurreição)
  bool     eventDeath = false;     // acabou de morrer (som/vibração)
  uint16_t hourGood[24] = {0};
  uint16_t hourTotal[24] = {0};

  void begin(Preferences &prefs) {
    _prefs = &prefs;
    hp            = _prefs->getFloat ("hp", 100.0f);
    exp           = _prefs->getULong ("exp", 0);
    streakDays    = _prefs->getUShort("streak", 0);
    goodSecsToday = _prefs->getULong ("goodsecs", 0);
    missionDone   = _prefs->getBool  ("mission", false);
    dead          = _prefs->getBool  ("dead", false);
    deadRes       = _prefs->getULong ("deadres", 0);
    deaths        = _prefs->getUShort("deaths", 0);
    dead          = _prefs->getBool  ("dead", false);
    deadRes       = _prefs->getULong ("deadres", 0);
    deaths        = _prefs->getUShort("deaths", 0);
    _lastDay      = _prefs->getUShort("lastday", 0);
    _minHpToday   = hp;
  }

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

  // ---- Tick 1 Hz ----
  bool tick(float angle, bool sleeping) {
    GotchiState prev = state;

    // -------- MODO FANTASMA: só a boa postura ressuscita --------
    if (dead) {
      state = ST_GHOST;
      if (!sleeping) {
        struct tm t; bool ht = getLocalTime(&t, 0);
        if (ht) hourTotal[t.tm_hour]++;
        if (angle < ANG_OK) {
          deadRes++;
          if (ht) hourGood[t.tm_hour]++;
          if (deadRes >= RES_SECS) resurrect();
        } else if (deadRes > 1) deadRes -= 2;
      }
      if (++_saveCounter >= 300) { save(); _saveCounter = 0; }
      return state != prev;
    }

    if (sleeping) { state = ST_SLEEP; return false; }

    struct tm t;
    bool haveTime = getLocalTime(&t, 0);
    int hh = haveTime ? t.tm_hour : -1;

    if (angle < ANG_OK) {
      hp = min(hp + HP_GAIN_OK, 100.0f);
      exp += EXP_PER_SEC;
      goodSecsToday++;
      hp0Secs = 0;
      if (hh >= 0) hourGood[hh]++;
      state = happySprite();
      if (!missionDone && goodSecsToday >= MISSION_MIN * 60UL) {
        missionDone = true;
        exp += MISSION_EXP;
        hp = min(hp + 1.0f, 100.0f);
        eventCelebrate = true;
      }
    } else {
      if (angle < ANG_WARN) { hp = max(hp - HP_LOSS_WARN, 0.0f); state = ST_WARNING; }
      else                  { hp = max(hp - HP_LOSS_BAD,  0.0f); state = ST_BAD; }
      // vida por um fio: HP esgotado + má postura acumula para a morte
      if (hp <= 0.0f && ++hp0Secs >= DEATH_SECS) die();
    }
    if (hh >= 0) hourTotal[hh]++;

    // vida por um fio: HP a 0 e má postura acumula para a morte
    if (hp <= 0.01f && angle >= ANG_OK) {
      if (++hp0Secs >= DEATH_SECS) {          // ☠️ morre
        dead = true; deaths++; hp0Secs = 0; deadRes = 0;
        streakDays = 0;                       // renasce como Ember
        exp /= 2;                             // metade da EXP perde-se
        eventDeath = true;
        state = ST_GHOST;
        save();
        return true;
      }
    } else if (angle < ANG_OK) hp0Secs = 0;

    _minHpToday = min(_minHpToday, hp);
    checkNewDay(haveTime ? t.tm_yday + 1 : 0);

    if (++_saveCounter >= 300) { save(); _saveCounter = 0; }
    return state != prev;
  }

  uint16_t level() const { return 1 + exp / 20000; }
  uint8_t scoreToday() const {
    uint32_t g = 0, tot = 0;
    for (int i = 0; i < 24; i++) { g += hourGood[i]; tot += hourTotal[i]; }
    return tot ? (uint8_t)(100UL * g / tot) : 100;
  }
  uint8_t hourScore(int h) const {
    return hourTotal[h] ? (uint8_t)(100UL * hourGood[h] / hourTotal[h]) : 255;
  }

  const char* stateText() const {
    switch (state) {
      case ST_GHOST:     return TR(K_FANTASMA);
      case ST_WARNING:   return TR(K_ATENCAO);
      case ST_BAD:       return TR(K_CRITICO);
      case ST_SLEEP:     return TR(K_DORMIR);
      case ST_CELEBRATE: return TR(K_FESTA);
      case ST_HAPPY:     return TR(K_FELIZ);
      default:           return phaseName();
    }
  }
  const char* postureText(float ang) const {
    if (ang < ANG_OK * 0.6f) return TR(K_P_EXC);
    if (ang < ANG_OK)        return TR(K_P_BOA);
    if (ang < ANG_WARN)      return TR(K_P_AT);
    return TR(K_P_MA);
  }

  void save() {
    _prefs->putFloat ("hp", hp);
    _prefs->putULong ("exp", exp);
    _prefs->putUShort("streak", streakDays);
    _prefs->putULong ("goodsecs", goodSecsToday);
    _prefs->putBool  ("mission", missionDone);
    _prefs->putBool  ("dead", dead);
    _prefs->putULong ("deadres", deadRes);
    _prefs->putUShort("deaths", deaths);
    _prefs->putBool  ("dead", dead);
    _prefs->putULong ("deadres", deadRes);
    _prefs->putUShort("deaths", deaths);
    _prefs->putUShort("lastday", _lastDay);
  }

private:
  Preferences *_prefs = nullptr;
  float    _minHpToday = 100.0f;
  uint16_t _lastDay = 0;
  uint16_t _saveCounter = 0;

  void die() {                     // ☠️ apagou-se
    dead = true; deaths++;
    deadRes = 0; hp0Secs = 0;
    streakDays = 0;                // o novo gotchi recomeça em Ember
    exp /= 2;                      // metade da EXP parte com ele
    eventDeath = true;
    save();
  }
  void resurrect() {               // 🔥 renasceu
    dead = false;
    hp = 50.0f; deadRes = 0; hp0Secs = 0;
    eventCelebrate = true;
    save();
  }

  void checkNewDay(uint16_t day) {
    if (day == 0) return;
    if (_lastDay == 0) { _lastDay = day; return; }
    if (day != _lastDay) {
      uint8_t oldPhase = phase();
      if (!dead) {
        if (_minHpToday >= STREAK_MIN_HP) streakDays++;
        else                              streakDays = 0;
        if (phase() != oldPhase) eventCelebrate = true;
      }
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
