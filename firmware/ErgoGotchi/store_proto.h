// ============================================================
// Ergo-Gotchi — store_proto.h (v6: dados da app guardados no colar)
//
// Lógica pura (sem Arduino) do transporte de um "bloco" de dados
// entre a app e o colar, em pacotes BLE de 20 bytes:
//
//   pacote de dados:  'D' + seq u16 LE + até 17 bytes   (app<->colar)
//   cabeçalho (colar->app): 'H' + estado u8 + tamanho u32 LE
//        estado: 0 = há dados, 1 = colar sem espaço de ficheiros,
//                2 = vazio (ainda sem dados)
//   confirmação (colar->app): 'K' + estado u8
//        estado: 0 = guardado, 1 = erro na transferência,
//                2 = falhou a escrita, 3 = sem espaço de ficheiros
//
// O colar não interpreta o bloco: a app envia-o já comprimido (gzip).
// Separado do BLE para poder ser testado no PC.
// ============================================================
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#define STORE_MAX    16384   // tamanho máximo do bloco (bytes, comprimido)
#define STORE_CHUNK  17      // bytes úteis por pacote
#define STORE_PKT    20      // 'D' + seq(2) + 17

namespace StoreProto {
  inline size_t header(uint8_t *o, uint8_t status, uint32_t len) {
    o[0] = 'H'; o[1] = status;
    o[2] = len; o[3] = len >> 8; o[4] = len >> 16; o[5] = len >> 24;
    return 6;
  }
  inline size_t ack(uint8_t *o, uint8_t status) { o[0] = 'K'; o[1] = status; return 2; }
}

// ---- Receção (app -> colar) ----
class StoreRx {
public:
  enum State : uint8_t { IDLE = 0, RECEIVING = 1, DONE = 2, FAILED = 3 };

  ~StoreRx() { free(_buf); }

  // novo envio. Se o tamanho for inválido fica FAILED (a app recebe erro).
  void begin(uint32_t len) {
    free(_buf); _buf = nullptr; _len = _got = 0; _seq = 0;
    if (len == 0 || len > STORE_MAX) { _st = FAILED; return; }
    _buf = (uint8_t *)malloc(len);
    if (!_buf) { _st = FAILED; return; }
    _len = len; _st = RECEIVING;
  }

  void chunk(const uint8_t *d, size_t n) {
    if (_st != RECEIVING) return;
    if (n < 4 || d[0] != 'D') { _st = FAILED; return; }
    uint16_t seq = d[1] | (d[2] << 8);
    size_t pl = n - 3;
    if (seq != _seq || pl > STORE_CHUNK || _got + pl > _len) { _st = FAILED; return; }
    memcpy(_buf + _got, d + 3, pl);
    _got += pl; _seq++;
    if (_got == _len) _st = DONE;
  }

  State          state() const { return _st; }
  const uint8_t *data()  const { return _buf; }
  uint32_t       size()  const { return _len; }

  // chamar no loop principal depois de tratar DONE/FAILED (ou ao desligar)
  void release() { free(_buf); _buf = nullptr; _len = _got = 0; _seq = 0; _st = IDLE; }

private:
  uint8_t *_buf = nullptr;
  uint32_t _len = 0, _got = 0;
  uint16_t _seq = 0;
  volatile State _st = IDLE;
};

// ---- Envio (colar -> app) ----
class StoreTx {
public:
  ~StoreTx() { free(_d); }

  // fica dono de d (malloc); é libertado no fim ou em stop()
  void start(uint8_t *d, uint32_t n) {
    free(_d); _d = d; _n = n; _pos = 0; _seq = 0; _on = (d && n > 0);
  }
  void stop() { free(_d); _d = nullptr; _on = false; }
  bool active() const { return _on; }

  // escreve o próximo pacote em out[STORE_PKT]; devolve o tamanho (0 = nada)
  size_t next(uint8_t *out) {
    if (!_on) return 0;
    size_t pl = _n - _pos;
    if (pl > STORE_CHUNK) pl = STORE_CHUNK;
    out[0] = 'D'; out[1] = _seq; out[2] = _seq >> 8;
    memcpy(out + 3, _d + _pos, pl);
    _pos += pl; _seq++;
    if (_pos >= _n) stop();
    return pl + 3;
  }

private:
  uint8_t *_d = nullptr;
  uint32_t _n = 0, _pos = 0;
  uint16_t _seq = 0;
  bool _on = false;
};
