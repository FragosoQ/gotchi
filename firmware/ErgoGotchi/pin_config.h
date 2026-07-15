// ============================================================
// Ergo-Gotchi — pin_config.h
// Placa: Waveshare ESP32-C6-Touch-AMOLED-1.8
// (SH8601 QSPI + FT3168 + QMI8658 + AXP2101 + XCA9554)
// ============================================================
#pragma once

#define XPOWERS_CHIP_AXP2101

// --- Display AMOLED SH8601 (QSPI) ---
#define LCD_SDIO0 1
#define LCD_SDIO1 2
#define LCD_SDIO2 3
#define LCD_SDIO3 4
#define LCD_SCLK  0
#define LCD_CS    5
#define LCD_WIDTH  368
#define LCD_HEIGHT 448

// --- I2C partilhado (touch FT3168, IMU QMI8658, AXP2101, XCA9554) ---
#define IIC_SDA 8
#define IIC_SCL 7
#define TP_INT  15

// --- Áudio ES8311 (I2S) — altifalante onboard ---
#define I2S_MCK_IO 19
#define I2S_BCK_IO 20
#define I2S_DI_IO  21
#define I2S_WS_IO  22
#define I2S_DO_IO  23

// --- Motor de vibração (opcional) ---
// A placa NÃO tem motor onboard. Liga um motor de moeda (via transístor)
// a um GPIO livre nos pads de expansão e define aqui. -1 = desativado.
#define HAPTIC_PIN -1

// ============================================================
// Configuração Ergo-Gotchi
// ============================================================

// --- Wi-Fi / Raspberry Pi (dashboard) ---
#define WIFI_SSID   "O_TEU_SSID"
#define WIFI_PASS   "A_TUA_PASS"
#define PI_HOST     "192.168.1.100"   // IP do Raspberry Pi
#define PI_PORT     5000
#define NET_ENABLED true              // false = colar 100% autónomo
#define NET_PUSH_MS 15000             // envia estado a cada 15 s

// --- Limiares de postura (graus) ---
#define ANG_OK      20.0f   // < 20°  -> Feliz
#define ANG_WARN    35.0f   // 20-35° -> Preocupado; > 35° -> Crítico

// --- Mecânica de jogo ---
#define HP_GAIN_OK    0.5f   // HP/s com boa postura
#define HP_LOSS_WARN  0.2f   // HP/s em aviso
#define HP_LOSS_BAD   1.0f   // HP/s em crítico
#define EXP_PER_SEC   10     // EXP/s com boa postura
#define STREAK_MIN_HP 95.0f  // HP mínimo do dia para contar streak
#define EVO_DAYS      30     // dias consecutivos para evoluir (Solarion)

// --- NTP (para relógio e contagem de dias) ---
#define NTP_SERVER  "pool.ntp.org"
#define TZ_INFO     "WET0WEST,M3.5.0/1,M10.5.0"   // Portugal continental

// --- Missão diária ---
#define MISSION_MIN 30      // minutos de boa postura para cumprir a missão
#define MISSION_EXP 1000    // recompensa

// --- Deteção de "a dormir" (colar imóvel) ---
#define SLEEP_STILL_SECS 300   // 5 min sem movimento -> sprite a dormir
