// ============================================================
// Ergo-Gotchi — touch_input.h
// Deteta swipe (esq/dir) e tap a partir das coordenadas do FT3168.
// O .ino alimenta feed() com os pontos; ao largar, gesture() devolve.
// ============================================================
#pragma once
#include <Arduino.h>

enum Gesture : uint8_t { G_NONE, G_TAP, G_SWIPE_LEFT, G_SWIPE_RIGHT };

class SwipeDetector {
public:
  void feed(int x, int y) {
    uint32_t now = millis();
    if (!_active) { _active = true; _x0 = x; _y0 = y; }
    _x1 = x; _y1 = y;
    _lastMs = now;
  }

  // Chamar a cada loop; devolve gesto quando o dedo larga (timeout)
  Gesture poll(int &tapX, int &tapY) {
    if (!_active || millis() - _lastMs < 150) return G_NONE;
    _active = false;
    int dx = _x1 - _x0, dy = _y1 - _y0;
    if (abs(dx) > 60 && abs(dx) > abs(dy)) {
      return dx < 0 ? G_SWIPE_LEFT : G_SWIPE_RIGHT;
    }
    if (abs(dx) < 20 && abs(dy) < 20) {
      tapX = _x1; tapY = _y1;
      return G_TAP;
    }
    return G_NONE;
  }

private:
  bool _active = false;
  int _x0 = 0, _y0 = 0, _x1 = 0, _y1 = 0;
  uint32_t _lastMs = 0;
};
