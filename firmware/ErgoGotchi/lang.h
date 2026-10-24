// ============================================================
// Ergo-Gotchi — lang.h : textos da UI em PT/EN
// Muda no ecrã CONFIG (linha "Idioma / Language"), fica em NVS.
// ============================================================
#pragma once
#include <stdint.h>

extern uint8_t UI_LANG;    // 0 = PT, 1 = EN (definido no .ino)

enum TrKey : uint8_t {
  K_P_EXC, K_P_BOA, K_P_AT, K_P_MA, K_HOJE,
  K_ANG_T, K_MTBOM, K_BOM, K_ATEN, K_MAU,
  K_MIS_T, K_MIS_MANTER, K_MIS_OK, K_MIS_REC,
  K_HIST_T, K_CFG_T, K_BRILHO, K_VIBRA, K_SOM, K_VOL, K_CALIB, K_IDIOMA, K_LIGADO,
  K_DIREITO, K_ASSENTE, K_CALIBRADO,
  K_FELIZ, K_ATENCAO, K_CRITICO, K_DORMIR, K_FESTA, K_FANTASMA, K_REBIRTH,
  K_SPLASH,
  N_KEYS
};

static const char* const TRS[N_KEYS][2] = {
  {"Postura Excelente", "Excellent Posture"},
  {"Postura Boa",       "Good Posture"},
  {"Atencao a Postura", "Watch Your Posture"},
  {"Postura Ma",        "Bad Posture"},
  {"Hoje",              "Today"},
  {"Angulo Cervical",   "Cervical Angle"},
  {"Muito Bom",         "Very Good"},
  {"Bom",               "Good"},
  {"Atencao",           "Careful"},
  {"Mau",               "Bad"},
  {"Missao Diaria",     "Daily Mission"},
  {"Manter postura %d min", "Keep posture %d min"},
  {"CUMPRIDA! +%d EXP", "DONE! +%d EXP"},
  {"Recompensa: +%d EXP", "Reward: +%d EXP"},
  {"Historico (Hoje)",  "History (Today)"},
  {"Config",            "Settings"},
  {"Brilho",            "Brightness"},
  {"Vibracao",          "Vibration"},
  {"Som",               "Sound"},
  {"Volume",            "Volume"},
  {"Calibrar postura",  "Calibrate posture"},
  {"Idioma",            "Language"},
  {"Ligado",            "Connected"},
  {"Poe-te direito!",   "Sit up straight!"},
  {"colar assente no peito", "pendant resting on chest"},
  {"CALIBRADO",         "CALIBRATED"},
  {"FELIZ",             "HAPPY"},
  {"ATENCAO",           "CAREFUL"},
  {"CRITICO",           "CRITICAL"},
  {"A DORMIR",          "SLEEPING"},
  {"FESTA!",            "PARTY!"},
  {"FANTASMA",          "GHOST"},
  {"RENASCER %d/%d min","REBIRTH %d/%d min"},
  {"A chama vive da tua postura", "The flame lives on your posture"},
};
#define TR(k) TRS[k][UI_LANG]
