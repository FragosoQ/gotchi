// ============================================================
// Ergo-Gotchi — sound.h
// Sons de alerta pelo altifalante onboard (ES8311 + I2S).
//
//   sndWarning()  — 2 tons descendentes (FELIZ -> ATENÇÃO)
//   sndCritical() — 3 bips graves urgentes (-> CRÍTICO)
//   sndRecover()  — chirp ascendente (volta a FELIZ)
//   sndSuccess()  — arpejo alegre (missão cumprida / evolução)
//
// Baseado no exemplo oficial 15_ES8311 da Waveshare.
// O amplificador é ligado pelo pino 7 do expander XCA9554.
// ============================================================
#pragma once
#include "ESP_I2S.h"
#include <Adafruit_XCA9554.h>
#include "es8311.h"
#include "pin_config.h"

#define SND_RATE   16000
#define SND_VOLUME 80        // 0-100 no codec

class GotchiSound {
public:
  bool begin(Adafruit_XCA9554 &expander) {
    _i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
    if (!_i2s.begin(I2S_MODE_STD, SND_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                    I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
      return false;
    }
    // Amplificador ON
    expander.pinMode(7, OUTPUT);
    expander.digitalWrite(7, 1);

    _es = es8311_create(0, ES8311_ADDRRES_0);
    if (!_es) return false;
    es8311_handle_t es = _es;
    const es8311_clock_config_t clk = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = SND_RATE * 256,
      .sample_frequency = SND_RATE
    };
    if (es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
    es8311_sample_frequency_config(es, SND_RATE * 256, SND_RATE);
    es8311_microphone_config(es, false);
    es8311_voice_volume_set(es, SND_VOLUME, NULL);
    _ok = true;
    return true;
  }

  bool ready() const { return _ok; }

  // Volume por hardware no codec (0-100)
  void setVolume(uint8_t v) {
    if (!_ok) return;
    es8311_voice_volume_set(_es, constrain(v, 0, 100), NULL);
  }

  // Tom senoidal com fade-in/out (evita estalidos). Bloqueia ~ms.
  void tone(float freq, uint16_t ms, float vol = 0.5f) {
    if (!_ok) return;
    const int total = SND_RATE * ms / 1000;
    const int fade  = SND_RATE * 6 / 1000;          // 6 ms de rampa
    static int16_t buf[256 * 2];                    // 256 amostras estéreo
    float ph = 0, dph = 2.0f * PI * freq / SND_RATE;
    for (int done = 0; done < total; ) {
      int n = min(256, total - done);
      for (int i = 0; i < n; i++) {
        int s = done + i;
        float env = 1.0f;
        if (s < fade)          env = (float)s / fade;
        if (total - s < fade)  env = (float)(total - s) / fade;
        int16_t v = (int16_t)(sinf(ph) * 32767.0f * vol * env);
        buf[i * 2] = v; buf[i * 2 + 1] = v;
        ph += dph; if (ph > 2 * PI) ph -= 2 * PI;
      }
      _i2s.write((uint8_t *)buf, n * 4);
      done += n;
    }
  }

  void pause(uint16_t ms) { delay(ms); }

  // ---- Melodias ----
  void sndWarning()  { tone(880, 110, .45f); pause(30); tone(660, 140, .45f); }
  void sndCritical() { for (int i = 0; i < 3; i++) { tone(440, 120, .6f); pause(60); } }
  void sndRecover()  { tone(660, 80, .35f); tone(990, 120, .35f); }
  void sndSuccess()  { tone(659, 90, .5f); tone(784, 90, .5f); tone(1047, 180, .5f); }

private:
  I2SClass _i2s;
  es8311_handle_t _es = NULL;
  bool _ok = false;
};
