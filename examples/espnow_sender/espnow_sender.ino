// Пример передатчика для MONSTRIX Head: обороты и температура по ESP-NOW.
// Любой ESP32 (Arduino: «ESP32 Dev Module», ядро ESP32 by Espressif 3.x). Протокол — PROTOCOL-ESPNOW.md.
//
// Голова видит поток → средней кнопкой листает тахометр и температуру.
// Здесь значения — синусом и медленным прогревом, чтобы проверить на столе.
// Свои данные (CAN, OBD, датчик) — подставь в readRpm() / readCoolant().

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static const uint8_t CHANNEL = 1;    // голова слушает канал 1
static const uint8_t MAGIC = 0x4D;   // 'M'
static const uint8_t ID_RPM = 100, ID_SPEED = 101, ID_COOLANT = 102;

typedef struct __attribute__((packed)) {
  uint8_t magic, type, count;
  uint32_t seq;
} MsgHead;

typedef struct __attribute__((packed)) {
  uint8_t id;
  int32_t val;  // × 10^dec; здесь dec = 0
} SigValue;

typedef struct __attribute__((packed)) {
  uint8_t id;
  char name[10];
  char unit[6];
  uint8_t dec, src;
  int32_t vmin, vmax;
} SigName;

static uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint32_t seq = 0;

// ── свои данные ──
int32_t readRpm() { return 850 + (int32_t)(2200 * powf(sinf(millis() / 2000.0f), 2)); }
int32_t readCoolant() { return min(90, 40 + (int)(millis() / 2000)); }  // +1° каждые 2 с
int32_t readSpeed() { return readRpm() > 1500 ? (readRpm() - 850) / 40 : 0; }

void sendValues() {
  uint8_t buf[sizeof(MsgHead) + 3 * sizeof(SigValue)];
  MsgHead* h = (MsgHead*)buf;
  h->magic = MAGIC;
  h->type = 1;  // значения
  h->count = 3;
  h->seq = ++seq;
  SigValue* v = (SigValue*)(buf + sizeof(MsgHead));
  v[0] = {ID_RPM, readRpm()};
  v[1] = {ID_COOLANT, readCoolant()};
  v[2] = {ID_SPEED, readSpeed()};
  esp_now_send(BCAST, buf, sizeof(buf));
}

void sendNames() {  // подписи — по желанию: без них голова считает dec = 0
  uint8_t buf[sizeof(MsgHead) + 3 * sizeof(SigName)] = {};
  MsgHead* h = (MsgHead*)buf;
  h->magic = MAGIC;
  h->type = 2;  // подписи
  h->count = 3;
  h->seq = ++seq;
  SigName* n = (SigName*)(buf + sizeof(MsgHead));
  n[0] = {ID_RPM, "RPM", "rpm", 0, 0, 0, 7000};
  n[1] = {ID_COOLANT, "COOLANT", "C", 0, 0, 40, 120};
  n[2] = {ID_SPEED, "SPEED", "km/h", 0, 0, 0, 180};
  esp_now_send(BCAST, buf, sizeof(buf));
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW не запустился");
    return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  Serial.println("Шлю обороты и температуру на канале 1");
}

void loop() {
  static uint32_t tv = 0, tn = 0;
  uint32_t now = millis();
  if (now - tv >= 50) {  // 20 раз в секунду
    tv = now;
    sendValues();
  }
  if (now - tn >= 500) {
    tn = now;
    sendNames();
  }
}
