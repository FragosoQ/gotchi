// ============================================================
//  🔥 ERGO-GOTCHI v3 — Colar de Postura
//  Waveshare ESP32-C6-Touch-AMOLED-1.8 (368x448, SH8601)
//
//  "A chama vive da tua postura. Alimenta-a com ergonomia."
//
//  v3: 5 ecrãs (swipe): HOME | ANGULO | MISSAO | HISTORICO | CONFIG
//      8 estados de sprite, 4 fases de evolução:
//      EMBER -> SOLARION (30d) -> GUARDIAN (90d) -> PHOENIX (180d)
//      Missão diária (30 min), histórico horário, modo dormir.
//
//  Botão BOOT: calibrar postura "zero".
//  Bibliotecas: as do repo oficial Waveshare (ver README).
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include <Adafruit_XCA9554.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

#include "pin_config.h"
#include "sprites.h"
#include "game_logic.h"
#include "imu_posture.h"
#include "ui_draw.h"
#include "touch_input.h"
#include "net_link.h"
#include "sound.h"

// ---- Hardware ----
Adafruit_XCA9554 expander;
XPowersPMU PMU;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_SH8601 *gfx = new Arduino_SH8601(
    bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT);

std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus =
    std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
void Touch_Interrupt(void);
std::unique_ptr<Arduino_IIC> FT3168(new Arduino_FT3x68(
    IIC_Bus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, Touch_Interrupt));
void Touch_Interrupt(void) { FT3168->IIC_Interrupt_Flag = true; }

// ---- Módulos ----
Preferences   prefs;
PostureSensor imu;
GotchiGame    game;
GotchiUI      ui(gfx);
SwipeDetector swipe;
NetLink       net;
GotchiSound   snd;

// ---- Estado da app ----
Screen   screen = SCR_HOME;
uint32_t tAnim = 0, tTick = 0, tHud = 0, celebrateUntil = 0;
uint8_t  frame = 0;
bool     bootWasDown = false;
int      lastBatt = -1;

int battery() { return PMU.isBatteryConnect() ? PMU.getBatteryPercent() : -1; }

void haptic(uint16_t ms, uint8_t pulses = 1) {
  if (HAPTIC_PIN < 0 || !ui.cfg.vibra) return;
  for (uint8_t i = 0; i < pulses; i++) {
    digitalWrite(HAPTIC_PIN, HIGH); delay(ms);
    digitalWrite(HAPTIC_PIN, LOW);  if (i + 1 < pulses) delay(80);
  }
}

// Sprite mostrado (celebração sobrepõe o estado do jogo)
GotchiState displayState() {
  if (millis() < celebrateUntil) return ST_CELEBRATE;
  return game.state;
}

void drawScreen() {
  float ang = imu.effectiveAngle();
  switch (screen) {
    case SCR_HOME:      ui.drawHome(game, ang, lastBatt, net.connected(), frame); break;
    case SCR_ANGULO:    ui.drawAngulo(game, ang, lastBatt, net.connected());      break;
    case SCR_MISSAO:    ui.drawMissao(game, frame, lastBatt, net.connected());    break;
    case SCR_HISTORICO: ui.drawHistorico(game, lastBatt, net.connected());        break;
    case SCR_CONFIG:    ui.drawConfig(net.connected(), lastBatt, net.ip().c_str()); break;
    default: break;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(IIC_SDA, IIC_SCL);

  if (expander.begin(0x20)) {
    expander.pinMode(4, OUTPUT);  expander.pinMode(5, OUTPUT);
    expander.digitalWrite(4, 1);  expander.digitalWrite(5, 1);
  }
  delay(300);

  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);

  if (!gfx->begin()) Serial.println("[!] gfx falhou");
  if (!FT3168->begin()) Serial.println("[!] FT3168 falhou");

  prefs.begin("gotchi", false);
  game.begin(prefs);
  if (!imu.begin(prefs)) Serial.println("[!] QMI8658 falhou");
  net.begin(&game, &imu);
  if (!snd.begin(expander)) Serial.println("[!] ES8311/I2S falhou");

  // definições guardadas
  ui.cfg.brightness = prefs.getUChar("bright", 220);
  ui.cfg.volume     = prefs.getUChar("vol", 80);
  ui.cfg.vibra      = prefs.getBool("vibra", true);
  ui.cfg.som        = prefs.getBool("som", true);
  gfx->setBrightness(ui.cfg.brightness);
  snd.setVolume(ui.cfg.volume);

  if (HAPTIC_PIN >= 0) pinMode(HAPTIC_PIN, OUTPUT);
  pinMode(9, INPUT_PULLUP);   // BOOT

  // Splash
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(4); gfx->setTextColor(SPRITE_PALETTE[2]);
  gfx->setCursor(44, 190); gfx->print("ERGO'GOTCHI");
  gfx->setTextSize(2); gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(30, 240); gfx->print("A chama vive da tua postura");
  delay(1400);

  lastBatt = battery();
  drawScreen();
}

void loop() {
  uint32_t now = millis();
  imu.update();

  // ---- Botão BOOT: calibrar ----
  bool bootDown = digitalRead(9) == LOW;
  if (bootDown && !bootWasDown) {
    imu.calibrate();
    haptic(80);
    gfx->setTextSize(2); gfx->setTextColor(SPRITE_PALETTE[11]);
    gfx->setCursor(120, 32); gfx->print("CALIBRADO");
  }
  bootWasDown = bootDown;

  // ---- Touch: swipe entre ecrãs + taps no CONFIG ----
  if (FT3168->IIC_Interrupt_Flag) {
    FT3168->IIC_Interrupt_Flag = false;
    int32_t tx = FT3168->IIC_Read_Device_Value(FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
    int32_t ty = FT3168->IIC_Read_Device_Value(FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
    if (tx >= 0 && ty >= 0) swipe.feed(tx, ty);
  }
  int tapX, tapY;
  Gesture g = swipe.poll(tapX, tapY);
  if (g == G_SWIPE_LEFT)  { screen = (Screen)((screen + 1) % SCR_COUNT); drawScreen(); }
  if (g == G_SWIPE_RIGHT) { screen = (Screen)((screen + SCR_COUNT - 1) % SCR_COUNT); drawScreen(); }
  if (g == G_TAP && screen == SCR_CONFIG) {
    uint8_t what = ui.configTap(tapX, tapY);
    if (what) {
      gfx->setBrightness(ui.cfg.brightness);
      prefs.putUChar("bright", ui.cfg.brightness);
      prefs.putUChar("vol", ui.cfg.volume);
      prefs.putBool("vibra", ui.cfg.vibra);
      prefs.putBool("som", ui.cfg.som);
      haptic(30);
      if (what == 4) {                       // volume: aplica + bip de teste
        snd.setVolume(ui.cfg.volume);
        if (ui.cfg.som && ui.cfg.volume > 0) snd.tone(880, 90, .5f);
      }
    }
  }

  // ---- Tick de jogo: 1 Hz ----
  if (now - tTick >= 1000) {
    tTick = now;
    float ang = imu.effectiveAngle();
    bool sleeping = imu.stillFor(SLEEP_STILL_SECS);
    bool changed = game.tick(ang, sleeping);

    if (game.eventCelebrate) {            // missão cumprida / evolução!
      game.eventCelebrate = false;
      celebrateUntil = now + 6000;
      haptic(120, 3);
      if (ui.cfg.som) snd.sndSuccess();
      if (screen == SCR_HOME || screen == SCR_MISSAO) drawScreen();
    }

    // ---- Alertas escalonados por mudança de nível ----
    // nível: 0 = FELIZ (qualquer fase), 1 = ATENÇÃO, 2 = CRÍTICO
    static uint8_t prevLvl = 0;
    uint8_t lvl = game.state == ST_WARNING ? 1
                : game.state == ST_BAD     ? 2
                : game.state == ST_SLEEP   ? prevLvl   // dormir não alerta
                : 0;
    if (lvl != prevLvl) {
      if (lvl == 1 && prevLvl < 1) {          // FELIZ -> ATENÇÃO
        haptic(120);
        if (ui.cfg.som) snd.sndWarning();
      } else if (lvl == 2) {                  // -> CRÍTICO
        haptic(220, 3);
        if (ui.cfg.som) snd.sndCritical();
      } else if (lvl == 0 && prevLvl > 0) {   // recuperou -> FELIZ
        if (ui.cfg.som) snd.sndRecover();
      }
      prevLvl = lvl;
    }
    (void)changed;

    // atualizar zonas dinâmicas do ecrã atual
    switch (screen) {
      case SCR_HOME:   ui.homeDynamics(game, ang, false); break;
      case SCR_ANGULO: ui.anguloDynamics(game, ang);      break;
      case SCR_MISSAO: ui.missaoDynamics(game);           break;
      default: break;
    }
  }

  // ---- Animação do sprite: ~3 FPS (HOME e MISSAO) ----
  if (now - tAnim >= 333) {
    tAnim = now;
    frame = (frame + 1) % SPRITE_FRAMES;
    if (screen == SCR_HOME)
      ui.drawSprite(displayState(), frame, (LCD_WIDTH - 192) / 2, 46, 3);
    else if (screen == SCR_MISSAO)
      ui.drawSprite(millis() < celebrateUntil || game.missionDone ? ST_CELEBRATE : game.happySprite(),
                    frame, (LCD_WIDTH - 128) / 2, 250, 2);
  }

  // ---- HUD/bateria: 5 s ----
  if (now - tHud >= 5000) {
    tHud = now;
    lastBatt = battery();
    ui.drawHeader(lastBatt, net.connected());
    if (screen == SCR_HISTORICO) drawScreen();   // barras atualizam devagar
  }

  net.update();
  delay(8);
}
