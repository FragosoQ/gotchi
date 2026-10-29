// ============================================================
// Ergo-Gotchi — ble_link.h (v5: ligação Bluetooth à app)
//
// O colar anuncia-se como "ErgoGotchi" (BLE). A app, aberta em
// HTTPS no Chrome/Edge (PC ou Android), liga-se por Web Bluetooth:
//
//   ANGLE  (write)  <- ângulo da câmara: uint16 LE = graus x10
//                      (tem prioridade 10 s sobre o IMU)
//   STATUS (notify) -> estado do jogo, 20 bytes, 1x/s (ver abaixo)
//   CMD    (write)  <- 1 byte: 1 = calibrar
//                      ou 5 bytes: 0x54 ('T') + uint32 LE epoch UTC
//                      (acerta o relógio do colar, mesmo sem Wi-Fi)
//
// STATUS (little-endian):
//   0  u16 hp x10        2  u32 exp         6  u16 streak
//   8  u8  fase          9  u8  estado     10  u16 ângulo x10
//  12  u8  score        13  u8  flags (b0 missão, b1 fantasma,
//                                           b2 ângulo da câmara)
//  14  u16 deadRes (s)  16  u16 goodSecsToday   18 u8 bateria
//  19  u8  versão do protocolo (1)
//
// v6 — DADOS DA APP NO COLAR (resumo diário, conquistas, horas):
//   STORE  (write+notify)  e1f0a004…  pacotes de 20 B, ver store_proto.h
//   CMD    0x52 ('R')                 <- pede o bloco guardado; o colar
//                                        responde por STORE: cabeçalho 'H'
//                                        e depois pacotes 'D'
//   CMD    0x57 ('W') + u32 LE tamanho <- anuncia um envio; a app escreve
//                                        depois os pacotes 'D' em STORE; no
//                                        fim o colar guarda e responde 'K'
//   O colar guarda o bloco num ficheiro (FFat/LittleFS) sem o interpretar.
// ============================================================
#pragma once
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <sys/time.h>
#include "pin_config.h"
#include "game_logic.h"
#include "imu_posture.h"
#include "app_store.h"

#define BLE_NAME      "ErgoGotchi"
#define BLE_SVC_UUID  "e1f0a000-6f2a-4c1e-9b7d-2c0e7a90a001"
#define BLE_ANG_UUID  "e1f0a001-6f2a-4c1e-9b7d-2c0e7a90a001"
#define BLE_STA_UUID  "e1f0a002-6f2a-4c1e-9b7d-2c0e7a90a001"
#define BLE_CMD_UUID  "e1f0a003-6f2a-4c1e-9b7d-2c0e7a90a001"
#define BLE_STO_UUID  "e1f0a004-6f2a-4c1e-9b7d-2c0e7a90a001"

class BleLink : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
  void begin(GotchiGame *game, PostureSensor *imu) {
    _game = game; _imu = imu;
    _fsOk = _fs.begin();                    // ficheiros p/ os dados da app
    setenv("TZ", TZ_INFO, 1); tzset();      // hora local correta com ou sem Wi-Fi

    BLEDevice::init(BLE_NAME);
    _srv = BLEDevice::createServer();
    _srv->setCallbacks(this);
    BLEService *svc = _srv->createService(BLE_SVC_UUID);

    _ang = svc->createCharacteristic(BLE_ANG_UUID,
             BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    _ang->setCallbacks(this);

    _sta = svc->createCharacteristic(BLE_STA_UUID,
             BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
#if defined(CONFIG_BLUEDROID_ENABLED)
    _sta->addDescriptor(new BLE2902());     // com NimBLE é automático
#endif

    _cmd = svc->createCharacteristic(BLE_CMD_UUID,
             BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    _cmd->setCallbacks(this);

    _sto = svc->createCharacteristic(BLE_STO_UUID,
             BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR |
             BLECharacteristic::PROPERTY_NOTIFY);
    _sto->setCallbacks(this);
#if defined(CONFIG_BLUEDROID_ENABLED)
    _sto->addDescriptor(new BLE2902());
#endif

    svc->start();
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SVC_UUID);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
  }

  bool connected() const { return _conn; }

  // true uma vez quando a app pediu calibração (tratada no loop)
  bool takeCalibrateRequest() { bool r = _calReq; _calReq = false; return r; }

  // chamar a cada loop; envia o estado 1x/s e reanuncia após desligar
  void update(int batt) {
    uint32_t now = millis();
    if (_pendingAng >= 0) { _imu->setRemoteAngle(_pendingAng); _pendingAng = -1; }
    if (_pendingEpoch) {
      struct timeval tv = { (time_t)_pendingEpoch, 0 };
      settimeofday(&tv, nullptr);
      _pendingEpoch = 0;
    }
    if (_wasConn && !_conn && now - _discTs > 400) {   // reanunciar
      _wasConn = false;
      BLEDevice::startAdvertising();
    }
    if (_conn) _wasConn = true;
    if (_conn && now - _lastTx >= 1000) { _lastTx = now; sendStatus(batt); }
    storeUpdate();
  }

  // ---- callbacks (correm na task do Bluetooth: só guardam valores) ----
  using BLEServerCallbacks::onConnect;
  using BLEServerCallbacks::onDisconnect;
  using BLECharacteristicCallbacks::onWrite;
  void onConnect(BLEServer *) override    { _conn = true; _lastTx = 0; }
  void onDisconnect(BLEServer *) override { _conn = false; _discTs = millis(); }

  void onWrite(BLECharacteristic *c) override {
    String v = c->getValue();
    const uint8_t *d = (const uint8_t *)v.c_str();
    size_t n = v.length();
    if (c == _ang && n >= 2) {
      float a = (d[0] | (d[1] << 8)) / 10.0f;
      if (a >= 0 && a < 120) _pendingAng = a;
    } else if (c == _sto) {
      _rx.chunk(d, n);
    } else if (c == _cmd && n >= 1) {
      if (d[0] == 1) _calReq = true;
      else if (d[0] == 'R') _rdReq = true;
      else if (d[0] == 'W' && n >= 5) {
        uint32_t len = d[1] | (d[2] << 8) | (d[3] << 16) | ((uint32_t)d[4] << 24);
        _rx.begin(len);
      }
      else if (d[0] == 'T' && n >= 5) {
        uint32_t e = d[1] | (d[2] << 8) | (d[3] << 16) | ((uint32_t)d[4] << 24);
        if (e > 1700000000UL) _pendingEpoch = e;      // data plausível
      }
    }
  }

private:
  GotchiGame    *_game = nullptr;
  PostureSensor *_imu  = nullptr;
  BLEServer         *_srv = nullptr;
  BLECharacteristic *_ang = nullptr, *_sta = nullptr, *_cmd = nullptr, *_sto = nullptr;
  volatile bool     _conn = false, _calReq = false, _rdReq = false;
  AppStore  _fs;
  bool      _fsOk = false;
  StoreRx   _rx;
  StoreTx   _tx;
  volatile float    _pendingAng = -1;
  volatile uint32_t _pendingEpoch = 0;
  bool     _wasConn = false;
  uint32_t _lastTx = 0, _discTs = 0;

  static uint16_t clamp16(uint32_t v) { return v > 65535 ? 65535 : (uint16_t)v; }
  static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
  static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

  void stoNotify(const uint8_t *p, size_t n) { _sto->setValue((uint8_t *)p, n); _sto->notify(); }

  // pedido de leitura: cabeçalho agora, pacotes nas voltas seguintes do loop
  void startRead() {
    uint8_t h[6];
    uint32_t n = 0;
    uint8_t *b = _fsOk ? _fs.load(n) : nullptr;
    uint8_t st = !_fsOk ? 1 : (b ? 0 : 2);
    stoNotify(h, StoreProto::header(h, st, b ? n : 0));
    if (b) _tx.start(b, n);
  }

  void storeUpdate() {
    if (!_conn) {                              // ligação caiu: larga o que estava a meio
      _tx.stop();
      if (_rx.state() != StoreRx::IDLE) _rx.release();
      _rdReq = false;
      return;
    }
    if (_rdReq) { _rdReq = false; _tx.stop(); startRead(); }
    for (int i = 0; i < 2 && _tx.active(); i++) {   // ~2 pacotes por volta (8 ms)
      uint8_t p[STORE_PKT];
      size_t n = _tx.next(p);
      if (n) stoNotify(p, n);
    }
    StoreRx::State st = _rx.state();
    if (st == StoreRx::DONE) {
      uint8_t r = !_fsOk ? 3 : (_fs.save(_rx.data(), _rx.size()) ? 0 : 2);
      _rx.release();
      uint8_t a[2]; stoNotify(a, StoreProto::ack(a, r));
    } else if (st == StoreRx::FAILED) {
      _rx.release();
      uint8_t a[2]; stoNotify(a, StoreProto::ack(a, 1));
    }
  }

  void sendStatus(int batt) {
    uint8_t b[20] = {0};
    float hp = _game->hp; if (hp < 0) hp = 0;
    put16(b + 0, (uint16_t)(hp * 10 + 0.5f));
    put32(b + 2, _game->exp);
    put16(b + 6, _game->streakDays);
    b[8] = _game->phase();
    b[9] = (uint8_t)_game->state;
    float a = _imu->effectiveAngle(); if (a < 0) a = 0;
    put16(b + 10, (uint16_t)(a * 10 + 0.5f));
    b[12] = _game->scoreToday();
    b[13] = (_game->missionDone ? 1 : 0) | (_game->dead ? 2 : 0) | (_imu->remoteFresh() ? 4 : 0);
    put16(b + 14, clamp16(_game->deadRes));
    put16(b + 16, clamp16(_game->goodSecsToday));
    b[18] = batt < 0 ? 255 : (uint8_t)batt;
    b[19] = 1;
    _sta->setValue(b, sizeof(b));
    _sta->notify();
  }
};
