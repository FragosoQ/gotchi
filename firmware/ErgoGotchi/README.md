# 🔥 Ergo-Gotchi — Firmware do Colar

Firmware Arduino (v3) para o **Waveshare ESP32-C6-Touch-AMOLED-1.8** (368×448, SH8601).
A chama vive da tua postura: HP sobe com ângulo cervical <20°, desce acima de 35°.

**5 ecrãs** com navegação por **swipe** no touch:
`HOME` (sprite + HP + fase) · `ANGULO` (gauge) · `MISSAO` (30 min diários) ·
`HISTORICO` (barras/hora + score) · `CONFIG` (brilho, vibração, som — por toque).

**Fases de evolução** (dias de streak com HP>95%):
EMBER (0-29) → **SOLARION** (30) → **GUARDIAN** (90) → **PHOENIX** (180).
8 estados de sprite × 3 frames = 48 KB em Flash (happy, warning, critical,
sleeping, celebrate, solarion, guardian, phoenix). O modo *sleeping* ativa-se
após 5 min sem movimento; *celebrate* dispara ao cumprir a missão diária ou evoluir.

## Estrutura

| Ficheiro | Função |
|---|---|
| `ErgoGotchi.ino` | Setup + loop (anim. 3 FPS, tick 1 Hz, swipe entre ecrãs) |
| `pin_config.h` | Pinos da placa + **configuração Wi-Fi/limiar/jogo** |
| `sprites.h` | 24 frames 64×64 (4-bit, 48 KB) gerados por `gen_sprites.py` |
| `imu_posture.h` | QMI8658 → pitch filtrado + calibração |
| `game_logic.h` | HP/EXP/nível/streak/evolução + NVS |
| `ui_draw.h` | Render do sprite escalado 4× + barras/HUD |
| `net_link.h` | POST periódico para o dashboard Flask no Pi |
| `touch_input.h` | Deteção de swipe/tap (FT3168) |

## Instalação (Arduino IDE)

1. **Placa**: instala o core ESP32 (Boards Manager → "esp32" da Espressif, v3.x).
   Seleciona **ESP32C6 Dev Module**, Flash Size **16MB**, **USB CDC On Boot: Enabled**.
2. **Bibliotecas**: usa as do repositório oficial da Waveshare (garantem compatibilidade):
   [`waveshareteam/ESP32-C6-Touch-AMOLED-1.8`](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-1.8)
   → copia `examples/Arduino-v3.3.5/libraries/` para a tua pasta `Arduino/libraries/`:
   - `GFX_Library_for_Arduino` (Arduino_GFX com SH8601 QSPI)
   - `Adafruit_XCA9554` (I/O expander que liga o painel)
   - `SensorLib` (QMI8658)
   - `XPowersLib` (AXP2101 — bateria)
3. Edita `pin_config.h`: `WIFI_SSID`, `WIFI_PASS`, `PI_HOST` (ou `NET_ENABLED false`
   para funcionar 100% autónomo).
4. Compila e faz upload.

## Utilização

- **Botão BOOT (pressão curta)**: calibra a postura atual como 0° — faz isto
  sentado com as costas direitas, com o colar já pendurado.
- **Ângulo efetivo**: usa o IMU do colar; se o Raspberry Pi responder com
  `{"angle_cam": X}` (câmara MediaPipe), esse valor tem prioridade durante 10 s.
- **Vibração**: a placa não tem motor — liga um motor de moeda a um GPIO livre
  nos pads de expansão (via transístor) e define `HAPTIC_PIN`.
- **Persistência**: HP, EXP, streak e evolução sobrevivem a reboot/bateria.

## Protocolo (v3.3 — sem Raspberry Pi)

O colar é um **servidor HTTP** na rede Wi-Fi (porta 80). O IP aparece
no ecrã CONFIG do colar — é esse que se põe na app.

- `POST /api/camera` com `{"angle": 12.3}` → a app envia o ângulo da
  câmara; tem prioridade sobre o IMU durante 10 s (CORS aberto).
- `GET /api/status` → estado do jogo:

```json
{"hp": 87.5, "exp": 123450, "level": 7, "streak": 5, "phase": 1,
 "state": 5, "angle": 12.3, "score": 91, "mission": true, "src": "cam"}
```

## Regenerar sprites

`python3 gen_sprites.py` (Pillow + numpy) volta a criar `sprites.h`,
`sprites_preview.png` e os GIFs. Ajusta cores/expressões no script.

## Sons de alerta (v3.1)

O colar usa o **altifalante onboard** (ES8311 + I2S, driver oficial incluído no
sketch: `es8311.c/.h/es8311_reg.h`). Alertas escalonados por transição de nível:

| Transição | Vibração* | Som |
|---|---|---|
| FELIZ → ATENÇÃO | 1 pulso curto | 2 tons descendentes (880→660 Hz) |
| → CRÍTICO | 3 pulsos fortes | 3 bips graves urgentes (440 Hz) |
| volta a FELIZ | — | chirp ascendente |
| Missão / Evolução | 3 pulsos | arpejo alegre |

O toggle **Som** no ecrã CONFIG liga/desliga (persistido; agora ON por defeito).
*Vibração continua a exigir motor externo em `HAPTIC_PIN`.

## Extra: bibliotecas adicionais v3

Além das quatro do README original, o v3 usa `Arduino_DriveBus` (touch FT3168)
— também está em `examples/Arduino-v3.3.5/libraries/` do repo Waveshare.
