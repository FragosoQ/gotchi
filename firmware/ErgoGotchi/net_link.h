// ============================================================
// Ergo-Gotchi — net_link.h (v3.3: SEM Raspberry Pi)
//
// O colar é agora um mini servidor HTTP na tua rede Wi-Fi:
//
//   POST /api/camera  <- a app envia {"angle": 12.3}
//                        (câmara MediaPipe tem prioridade 10 s
//                         sobre o IMU — mais precisa ao PC)
//   GET  /api/status  -> estado do jogo em JSON (hp, exp, fase,
//                        score, missão...) para quem quiser ler
//
// O IP do colar aparece no ecrã CONFIG — é esse que pões na app.
// CORS aberto para o browser poder falar diretamente com o colar.
// ============================================================
#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "pin_config.h"
#include "game_logic.h"
#include "imu_posture.h"

class NetLink {
public:
  void begin(GotchiGame *game, PostureSensor *imu) {
    _game = game; _imu = imu;
    if (!NET_ENABLED) return;
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    configTzTime(TZ_INFO, NTP_SERVER);   // relógio p/ streak diário

    _srv.on("/api/camera", HTTP_POST,    [this]() { handleCamera(); });
    _srv.on("/api/camera", HTTP_OPTIONS, [this]() { cors(); _srv.send(204); });
    _srv.on("/api/status", HTTP_GET,     [this]() { handleStatus(); });
    _srv.onNotFound([this]() { cors(); _srv.send(404, "text/plain", "Ergo-Gotchi: rota desconhecida"); });
    _srv.begin();
  }

  bool connected() const { return WiFi.status() == WL_CONNECTED; }
  String ip() const { return connected() ? WiFi.localIP().toString() : String("---"); }

  // Chamar a cada loop (não bloqueia)
  void update() {
    if (NET_ENABLED && connected()) _srv.handleClient();
  }

private:
  WebServer _srv{80};
  GotchiGame    *_game = nullptr;
  PostureSensor *_imu  = nullptr;

  void cors() {
    _srv.sendHeader("Access-Control-Allow-Origin", "*");
    _srv.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    _srv.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  }

  // {"angle": 12.3} — parse minimalista, sem dependências
  void handleCamera() {
    cors();
    String b = _srv.arg("plain");
    float a = -1;
    int i = b.indexOf("angle");
    if (i >= 0) { i = b.indexOf(':', i); if (i >= 0) a = b.substring(i + 1).toFloat(); }
    if (a >= 0 && a < 120) {
      _imu->setRemoteAngle(a);
      _srv.send(200, "application/json", "{\"ok\":true}");
    } else {
      _srv.send(400, "application/json", "{\"ok\":false,\"erro\":\"angle em falta\"}");
    }
  }

  void handleStatus() {
    cors();
    char body[224];
    snprintf(body, sizeof(body),
             "{\"hp\":%.1f,\"exp\":%lu,\"level\":%u,\"streak\":%u,"
             "\"phase\":%u,\"state\":%u,\"angle\":%.1f,"
             "\"score\":%u,\"mission\":%s,\"src\":\"%s\"}",
             _game->hp, (unsigned long)_game->exp, _game->level(), _game->streakDays,
             _game->phase(), (unsigned)_game->state, _imu->effectiveAngle(),
             _game->scoreToday(), _game->missionDone ? "true" : "false",
             _imu->remoteFresh() ? "cam" : "imu");
    _srv.send(200, "application/json", body);
  }
};
