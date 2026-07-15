// ============================================================
// Ergo-Gotchi — imu_posture.h (v3.5: vetor de referência)
//
// Em vez de assumir um eixo "para a frente" (que muda conforme
// a orientação/ajuste do colar), guardamos o VETOR GRAVIDADE
// na posição calibrada e medimos o ângulo entre esse vetor e o
// atual:  ang = acos( g_atual · g_ref )
//
// Assim, qualquer inclinação em qualquer direção conta — e a
// calibração funciona seja qual for a posição da placa.
// ============================================================
#pragma once
#include <Wire.h>
#include "SensorQMI8658.hpp"
#include <Preferences.h>
#include "pin_config.h"

class PostureSensor {
public:
  bool begin(Preferences &prefs) {
    _prefs = &prefs;
    if (!_qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
      return false;
    }
    _qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                             SensorQMI8658::ACC_ODR_125Hz,
                             SensorQMI8658::LPF_MODE_0);
    _qmi.enableAccelerometer();
    // referência guardada (se existir)
    _refValid = _prefs->getBool("refv", false);
    if (_refValid) {
      _ref[0] = _prefs->getFloat("refx", 0);
      _ref[1] = _prefs->getFloat("refy", 0);
      _ref[2] = _prefs->getFloat("refz", -1);
    }
    return true;
  }

  // Chamar frequentemente (cada loop)
  void update() {
    if (!_qmi.getDataReady()) return;
    IMUdata acc;
    if (!_qmi.getAccelerometer(acc.x, acc.y, acc.z)) return;

    if (!_init) {
      _filt[0] = acc.x; _filt[1] = acc.y; _filt[2] = acc.z;
      _init = true;
    }
    // deteção de movimento: distância entre leitura e filtrado
    float dx = acc.x - _filt[0], dy = acc.y - _filt[1], dz = acc.z - _filt[2];
    if (sqrtf(dx * dx + dy * dy + dz * dz) > 0.06f) _lastMotion = millis();

    // filtro passa-baixo por eixo (suaviza respiração/passos)
    _filt[0] += 0.12f * (acc.x - _filt[0]);
    _filt[1] += 0.12f * (acc.y - _filt[1]);
    _filt[2] += 0.12f * (acc.z - _filt[2]);

    // sem calibração ainda: a 1.ª leitura estável vira referência provisória
    if (!_refValid && millis() > 2000) {
      _ref[0] = _filt[0]; _ref[1] = _filt[1]; _ref[2] = _filt[2];
      _refValid = true;   // provisória (não gravada em NVS)
    }
  }

  // Ângulo (graus) entre a gravidade atual e a calibrada
  float angle() const {
    if (!_init || !_refValid) return 0;
    float na = sqrtf(_filt[0]*_filt[0] + _filt[1]*_filt[1] + _filt[2]*_filt[2]);
    float nr = sqrtf(_ref[0]*_ref[0] + _ref[1]*_ref[1] + _ref[2]*_ref[2]);
    if (na < 0.3f || nr < 0.3f) return 0;   // queda livre/ruído: ignora
    float dot = (_filt[0]*_ref[0] + _filt[1]*_ref[1] + _filt[2]*_ref[2]) / (na * nr);
    dot = constrain(dot, -1.0f, 1.0f);
    return acosf(dot) * 180.0f / PI;
  }

  // Ângulo vindo da câmara (app), se fresco (<10 s)
  void setRemoteAngle(float a) { _remote = a; _remoteTs = millis(); }
  bool remoteFresh() const { return _remoteTs && millis() - _remoteTs < 10000; }
  float effectiveAngle() const { return remoteFresh() ? _remote : angle(); }

  // Calibrar: gravidade atual passa a ser a referência (0°)
  void calibrate() {
    if (!_init) return;
    _ref[0] = _filt[0]; _ref[1] = _filt[1]; _ref[2] = _filt[2];
    _refValid = true;
    _prefs->putBool ("refv", true);
    _prefs->putFloat("refx", _ref[0]);
    _prefs->putFloat("refy", _ref[1]);
    _prefs->putFloat("refz", _ref[2]);
  }

  // true se imóvel há 'secs' segundos (pousado / a dormir)
  bool stillFor(uint32_t secs) const {
    return _init && (millis() - _lastMotion) > secs * 1000UL;
  }

private:
  SensorQMI8658 _qmi;
  Preferences  *_prefs = nullptr;
  float _filt[3] = {0, 0, 0};
  float _ref[3]  = {0, 0, -1};
  bool  _refValid = false;
  float _remote = 0;
  uint32_t _remoteTs = 0;
  uint32_t _lastMotion = 0;
  bool _init = false;
};
