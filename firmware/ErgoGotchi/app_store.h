// ============================================================
// Ergo-Gotchi — app_store.h (v6)
// Um único ficheiro na flash com os dados da app (resumo diário,
// conquistas, estatísticas por hora). Usa FFat ou LittleFS, o que
// existir na tabela de partições (ver README).
// Escrita segura: grava num temporário e só depois substitui.
// ============================================================
#pragma once
#include <Arduino.h>
#include <FS.h>
#include <FFat.h>
#include <LittleFS.h>
#include "store_proto.h"

#define STORE_FILE "/appdata.bin"
#define STORE_TMP  "/appdata.tmp"

class AppStore {
public:
  // true se há sistema de ficheiros utilizável
  bool begin() {
    for (int fmt = 0; fmt < 2 && !_fs; fmt++) {      // 1.ª volta sem formatar
      if (FFat.begin(fmt == 1))          _fs = &FFat;
      else if (LittleFS.begin(fmt == 1)) _fs = &LittleFS;
    }
    if (_fs && !_fs->exists(STORE_FILE) && _fs->exists(STORE_TMP))
      _fs->rename(STORE_TMP, STORE_FILE);            // corte de energia a meio
    return _fs != nullptr;
  }
  bool ok() const { return _fs != nullptr; }

  // lê o ficheiro para memória (malloc); nullptr se não existe/vazio
  uint8_t *load(uint32_t &n) {
    n = 0;
    if (!_fs) return nullptr;
    File f = _fs->open(STORE_FILE, "r");
    if (!f) return nullptr;
    size_t sz = f.size();
    if (sz == 0 || sz > STORE_MAX) { f.close(); return nullptr; }
    uint8_t *b = (uint8_t *)malloc(sz);
    if (!b) { f.close(); return nullptr; }
    if (f.read(b, sz) != sz) { free(b); f.close(); return nullptr; }
    f.close();
    n = sz;
    return b;
  }

  bool save(const uint8_t *d, uint32_t n) {
    if (!_fs) return false;
    File f = _fs->open(STORE_TMP, "w");
    if (!f) return false;
    size_t w = f.write(d, n);
    f.close();
    if (w != n) { _fs->remove(STORE_TMP); return false; }
    _fs->remove(STORE_FILE);
    return _fs->rename(STORE_TMP, STORE_FILE);
  }

private:
  fs::FS *_fs = nullptr;
};
