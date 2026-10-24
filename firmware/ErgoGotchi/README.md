# 🔥 Ergo'Gotchi — Firmware do Colar (v5)

Firmware Arduino para o **Waveshare ESP32-C6-Touch-AMOLED-1.8** (368×448, SH8601).
A chama vive da tua postura: HP sobe com ângulo cervical <20°, desce acima de 20° e cai depressa acima de 35°.

**5 ecrãs** com navegação por **swipe**:
`HOME` (sprite + HP + fase) · `ANGULO` (gauge) · `MISSAO` (30 min diários) ·
`HISTORICO` (barras/hora + score) · `CONFIG` (Wi-Fi/IP, brilho, vibração, som,
volume, **calibrar**, **idioma PT/EN** — por toque).

**Fases de evolução** (dias de streak com HP ≥95%):
EMBER (0-29) → **SOLARION** (30) → **GUARDIAN** (90) → **PHOENIX** (180).

**9 estados de sprite × 3 frames** (64×64, 4-bit): happy, warning, critical,
sleeping, celebrate, solarion, guardian, phoenix e **ghost**. O modo *sleeping*
ativa-se após 5 min sem movimento; *celebrate* dispara ao cumprir a missão,
evoluir ou ressuscitar.

## 👻 Morte e ressurreição (v4)

| Evento | Regra | Efeito |
|---|---|---|
| Morre | 10 min (`DEATH_SECS`) com HP 0 e má postura | vira fantasma · streak a 0 · EXP a metade · som + vibração |
| Ressuscita | 30 min (`RES_SECS`) de boa postura enquanto fantasma | volta com 50 HP · celebração |

Enquanto é fantasma, o HOME mostra uma barra ciana de progresso ("RENASCER x/30 min");
má postura faz a barra recuar. Os valores estão em `pin_config.h`
(para testar depressa, põe `DEATH_SECS 30`).

## ⌚ Ligação à app por Bluetooth (v5)

O colar anuncia-se como **ErgoGotchi**. Na app (Chrome/Edge no PC ou Android)
carrega em **⌚ COLAR** no topo, ou em Config → **Ligar colar**, e escolhe-o na lista.

- **Um só Gotchi**: enquanto estão ligados, o jogo é o do colar (HP, EXP, streak,
  fantasma); a app mostra-o e guarda essa cópia.
- **Câmara → colar**: a app envia o ângulo da câmara 2× por segundo; tem
  prioridade sobre o IMU durante 10 s.
- **Relógio**: ao ligar, a app acerta a hora do colar (os dias/streak funcionam sem Wi-Fi).
- **Calibrar colar** a partir da app (Config), com 3 s para te endireitares.
- Se a ligação cair, o colar continua sozinho e a app tenta religar durante ~1 min.
- Não funciona no iPhone nem no Firefox (não suportam Web Bluetooth).

Serviço BLE `e1f0a000-6f2a-4c1e-9b7d-2c0e7a90a001`: `…a001` ângulo (write, uint16 = graus×10),
`…a002` estado (notify, 20 bytes, 1×/s), `…a003` comandos (write: `1` = calibrar,
`'T'`+epoch = acertar relógio). Detalhes no topo de `ble_link.h`.

## Estrutura

| Ficheiro | Função |
|---|---|
| `ErgoGotchi.ino` | Setup + loop (anim. 3 FPS, tick 1 Hz, swipe, eventos de morte/ressurreição) |
| `pin_config.h` | Pinos da placa (LCD, I2C, I2S) + **Wi-Fi, limiares e regras do jogo** |
| `sprites.h` | 27 frames 64×64 (4-bit) gerados por `gen_sprites.py` |
| `imu_posture.h` | QMI8658 → ângulo pelo vetor gravidade + calibração (NVS) |
| `game_logic.h` | HP/EXP/nível/streak/evolução/morte + NVS |
| `ui_draw.h` | Render dos 5 ecrãs (textos via `lang.h`) |
| `lang.h` | Dicionário PT/EN (`TR(K_...)`) |
| `sound.h` | Sons pelo ES8311 + controlo de volume |
| `ble_link.h` | Bluetooth (BLE) para ligar à app |
| `net_link.h` | Servidor HTTP (API local) |
| `touch_input.h` | Deteção de swipe/tap (FT3168) |
| `es8311.c/.h`, `es8311_reg.h` | Driver oficial do codec áudio |
| `gen_sprites.py`, `render_mock.py` | Geração de sprites e mockups dos ecrãs |

## Instalação (Arduino IDE)

1. **Placa**: core ESP32 da Espressif v3.x. Seleciona **ESP32C6 Dev Module**,
   Flash Size **16MB**, **USB CDC On Boot: Enabled**.
   Partition Scheme com app ≥ 3 MB (Wi-Fi + Bluetooth ocupam mais espaço).
2. **Bibliotecas**: as do repositório oficial da Waveshare
   [`waveshareteam/ESP32-C6-Touch-AMOLED-1.8`](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-1.8)
   → copia `examples/Arduino-v3.3.5/libraries/` para `Arduino/libraries/`:
   `GFX_Library_for_Arduino`, `Arduino_DriveBus`, `Adafruit_XCA9554`,
   `SensorLib`, `XPowersLib`.
3. Edita `pin_config.h`: `WIFI_SSID` e `WIFI_PASS` (ou `NET_ENABLED false`
   para funcionar 100% autónomo). **Não faças commit da tua password.**
4. Compila e faz upload.

## Utilização

- **Calibrar**: CONFIG → Calibrar (contagem 3-2-1) ou botão BOOT. Faz isto
  sentado direito, com o colar já ajustado. O colar guarda a posição como 0°,
  por isso volta a calibrar se mudares o ajuste do fio.
- **Idioma**: CONFIG → Idioma alterna PT/EN (fica guardado).
- **Vibração**: a placa não tem motor — liga um motor de moeda a um GPIO livre
  (via transístor) e define `HAPTIC_PIN`.
- **Persistência**: HP, EXP, streak, evolução, estado de fantasma, calibração e
  idioma sobrevivem a reboot/bateria.
- O IMU não deteta bem o queixo ao peito — a câmara da app deteta.

## Sons de alerta

| Transição | Vibração* | Som |
|---|---|---|
| FELIZ → ATENÇÃO | 1 pulso curto | 2 tons descendentes |
| → CRÍTICO | 3 pulsos fortes | 3 bips graves |
| volta a FELIZ | — | chirp ascendente |
| Missão / Evolução / Ressurreição | 3 pulsos | arpejo alegre |
| Morte | 2 pulsos longos | melodia descendente |

O toggle **Som** e o **Volume** estão no ecrã CONFIG.
*Vibração exige motor externo em `HAPTIC_PIN`.

## API local (opcional)

Com Wi-Fi ligado, o colar é um servidor HTTP (porta 80); o IP aparece no ecrã CONFIG.

- `POST /api/camera` com `{"angle": 12.3}` → ângulo externo, com prioridade
  sobre o IMU durante 10 s (CORS aberto).
- `GET /api/status` → estado do jogo em JSON (`hp`, `exp`, `level`, `streak`,
  `phase`, `state`, `angle`, `score`, `mission`, `src`).

> A app no GitHub Pages (HTTPS) não consegue chamar o colar por HTTP local
> (o browser bloqueia *mixed content*), por isso o colar funciona de forma autónoma.

## Regenerar sprites

`python3 gen_sprites.py` (Pillow + numpy) volta a criar `sprites.h`,
`sprites_preview.png` e os GIFs.
⚠️ O `gen_sprites.py` incluído ainda não gera o fantasma — não regeneres
`sprites.h` com ele, ou perdes o estado ghost.
