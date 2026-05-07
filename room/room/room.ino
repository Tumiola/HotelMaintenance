// =========================
// ROOM NODE (ESP32 + RN2483A)
// Arduino IDE
// =========================
#include <Arduino.h>

HardwareSerial rn(2);

// ---------- Pin config ----------
static const int RN_RX_PIN  = 16; // ESP32 RX  <- RN2483 TX
static const int RN_TX_PIN  = 17; // ESP32 TX  -> RN2483 RX
static const int RN_RST_PIN = 4;  // ESP32 GPIO -> RN2483 RESET (active low)

// ---------- Node identity ----------
static const uint8_t  BUILDING_ID = 1;
static const uint8_t  FLOOR_ID    = 3;
static const uint16_t ROOM_ID     = 305;

// ---------- Timing ----------
static const uint32_t SUMMARY_PERIOD_MS = 30000;
static const uint32_t ACK_TIMEOUT_MS    = 2500;
static const uint8_t  MAX_RETRIES       = 3;

// ---------- Shared key (must match receiver) ----------
static const uint32_t XTEA_KEY[4] = {
  0xA56BABCD, 0x000FF123, 0x13572468, 0x89ABCDEF
};

// ---------- Packet ----------
static const uint8_t PACKET_LEN = 24;
static const uint8_t MAGIC      = 0x42;
static const uint8_t VERSION    = 0x01;
static const uint8_t TYPE_DATA  = 0x01;
static const uint8_t TYPE_ACK   = 0x02;

uint16_t seqNo = 0;
uint16_t testCounter = 0;
uint32_t nextSendAt = 0;

// ---------- Utilities ----------
void writeU16LE(uint8_t *p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
}

void writeU32LE(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

uint16_t readU16LE(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint32_t readU32LE(const uint8_t *p) {
  return (uint32_t)p[0] |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else crc <<= 1;
    }
  }
  return crc;
}

void xteaEncryptBlock(uint8_t *block) {
  uint32_t v0 = readU32LE(block);
  uint32_t v1 = readU32LE(block + 4);
  uint32_t sum = 0;
  const uint32_t delta = 0x9E3779B9;

  for (uint8_t i = 0; i < 32; i++) {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + XTEA_KEY[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + XTEA_KEY[(sum >> 11) & 3]);
  }

  writeU32LE(block, v0);
  writeU32LE(block + 4, v1);
}

void xteaDecryptBlock(uint8_t *block) {
  uint32_t v0 = readU32LE(block);
  uint32_t v1 = readU32LE(block + 4);
  const uint32_t delta = 0x9E3779B9;
  uint32_t sum = delta * 32;

  for (uint8_t i = 0; i < 32; i++) {
    v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + XTEA_KEY[(sum >> 11) & 3]);
    sum -= delta;
    v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + XTEA_KEY[sum & 3]);
  }

  writeU32LE(block, v0);
  writeU32LE(block + 4, v1);
}

void encryptPacket(uint8_t *packet) {
  for (uint8_t i = 0; i < PACKET_LEN; i += 8) xteaEncryptBlock(packet + i);
}

void decryptPacket(uint8_t *packet) {
  for (uint8_t i = 0; i < PACKET_LEN; i += 8) xteaDecryptBlock(packet + i);
}

String bytesToHex(const uint8_t *data, size_t len) {
  const char *hex = "0123456789ABCDEF";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    out += hex[(data[i] >> 4) & 0x0F];
    out += hex[data[i] & 0x0F];
  }
  return out;
}

bool hexToBytes(const String &hex, uint8_t *out, size_t expectedLen) {
  if (hex.length() != expectedLen * 2) return false;
  for (size_t i = 0; i < expectedLen; i++) {
    char c1 = hex[2 * i];
    char c2 = hex[2 * i + 1];
    auto nibble = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      return -1;
    };
    int hi = nibble(c1);
    int lo = nibble(c2);
    if (hi < 0 || lo < 0) return false;
    out[i] = (hi << 4) | lo;
  }
  return true;
}

// ---------- RN2483 serial helpers ----------
void rnWriteLine(const String &cmd) {
  rn.print(cmd);
  rn.print("\r\n");
  Serial.print(">> ");
  Serial.println(cmd);
}

String rnReadLine(uint32_t timeoutMs) {
  String line;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (rn.available()) {
      char c = (char)rn.read();
      if (c == '\r') continue;
      if (c == '\n') {
        if (line.length()) {
          Serial.print("<< ");
          Serial.println(line);
          return line;
        }
      } else {
        line += c;
      }
    }
    delay(1);
  }
  return "";
}

void rnFlush(uint32_t ms = 200) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    while (rn.available()) rn.read();
    delay(1);
  }
}

bool rnExpectOk(const String &cmd, uint32_t timeoutMs = 1500) {
  rnFlush();
  rnWriteLine(cmd);
  String line = rnReadLine(timeoutMs);
  return (line == "ok");
}

bool rnReset() {
  pinMode(RN_RST_PIN, OUTPUT);
  digitalWrite(RN_RST_PIN, HIGH);
  delay(10);
  digitalWrite(RN_RST_PIN, LOW);
  delay(50);
  digitalWrite(RN_RST_PIN, HIGH);
  delay(200);

  rnWriteLine("sys reset");
  String line = rnReadLine(1500); // version banner
  return line.length() > 0;
}

bool rnConfigure() {
  if (!rnReset()) return false;

  rnWriteLine("mac pause");
  String pauseResp = rnReadLine(1500);
  if (pauseResp.length() == 0) return false;

  if (!rnExpectOk("radio set mod lora")) return false;
  if (!rnExpectOk("radio set freq 868100000")) return false;
  if (!rnExpectOk("radio set pwr 14")) return false;
  if (!rnExpectOk("radio set sf sf7")) return false;
  if (!rnExpectOk("radio set bw 125")) return false;
  if (!rnExpectOk("radio set cr 4/5")) return false;
  if (!rnExpectOk("radio set crc on")) return false;
  if (!rnExpectOk("radio set wdt 0")) return false;
  if (!rnExpectOk("radio set iqi off")) return false;

  return true;
}

bool rnStartRx() {
  rnFlush();
  rnWriteLine("radio rx 0");
  String line = rnReadLine(1000);
  return (line == "ok");
}

bool rnTxHex(const String &hexPayload, uint32_t txDoneTimeoutMs = 7000) {
  rnFlush();
  rnWriteLine("radio tx " + hexPayload);

  String line = rnReadLine(1500);
  if (line != "ok") return false;

  uint32_t start = millis();
  while (millis() - start < txDoneTimeoutMs) {
    line = rnReadLine(500);
    if (line == "radio_tx_ok") return true;
    if (line.startsWith("radio_err") || line.startsWith("busy") || line.startsWith("invalid_param")) {
      return false;
    }
  }
  return false;
}

// ---------- Packet handling ----------
void buildDataPacket(uint8_t *packet, uint16_t seq, uint16_t testValue) {
  memset(packet, 0, PACKET_LEN);

  packet[0] = MAGIC;
  packet[1] = VERSION;
  packet[2] = TYPE_DATA;
  packet[3] = BUILDING_ID;
  packet[4] = FLOOR_ID;
  writeU16LE(packet + 5, ROOM_ID);
  writeU16LE(packet + 7, seq);
  writeU16LE(packet + 9, testValue);
  writeU32LE(packet + 11, millis() / 1000);
  writeU32LE(packet + 15, (uint32_t)esp_random());
  packet[19] = 0x01; // flags / reserved
  packet[20] = 0x00;
  packet[21] = 0x00;

  uint16_t crc = crc16_ccitt(packet, 22);
  writeU16LE(packet + 22, crc);

  encryptPacket(packet);
}

bool decodeAndValidate(uint8_t *packet) {
  decryptPacket(packet);

  if (packet[0] != MAGIC) return false;
  if (packet[1] != VERSION) return false;

  uint16_t rxCrc = readU16LE(packet + 22);
  uint16_t calcCrc = crc16_ccitt(packet, 22);
  return (rxCrc == calcCrc);
}

bool waitForAck(uint16_t expectedSeq) {
  if (!rnStartRx()) return false;

  uint32_t start = millis();
  while (millis() - start < ACK_TIMEOUT_MS) {
    String line = rnReadLine(200);
    if (!line.length()) continue;

    if (line == "radio_err") {
      rnStartRx();
      continue;
    }

    if (line.startsWith("radio_rx ")) {
      String hex = line.substring(9);
      uint8_t packet[PACKET_LEN];

      if (!hexToBytes(hex, packet, PACKET_LEN)) continue;
      if (!decodeAndValidate(packet)) continue;

      uint8_t  type      = packet[2];
      uint8_t  building  = packet[3];
      uint8_t  floor     = packet[4];
      uint16_t room      = readU16LE(packet + 5);
      uint16_t seq       = readU16LE(packet + 7);

      if (type == TYPE_ACK &&
          building == BUILDING_ID &&
          floor == FLOOR_ID &&
          room == ROOM_ID &&
          seq == expectedSeq) {
        Serial.printf("ACK received for seq=%u\n", seq);
        return true;
      }
    }
  }
  return false;
}

void scheduleNextSend() {
  uint32_t deterministicOffset = (ROOM_ID * 137UL) % 5000UL;
  uint32_t randomJitter = random(0, 2000);
  nextSendAt = millis() + SUMMARY_PERIOD_MS + deterministicOffset + randomJitter;
}

void sendSummaryOnce() {
  uint16_t thisSeq = ++seqNo;
  uint16_t testValue = ++testCounter;

  uint8_t packet[PACKET_LEN];
  buildDataPacket(packet, thisSeq, testValue);
  String hexPayload = bytesToHex(packet, PACKET_LEN);

  for (uint8_t attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    Serial.printf("Sending seq=%u attempt=%u value=%u\n", thisSeq, attempt, testValue);

    if (!rnTxHex(hexPayload)) {
      Serial.println("TX failed");
      delay(random(150, 600));
      continue;
    }

    if (waitForAck(thisSeq)) {
      Serial.println("Packet delivered");
      scheduleNextSend();
      return;
    }

    Serial.println("ACK timeout");
    delay(random(200, 900));
  }

  Serial.println("Delivery failed after retries");
  scheduleNextSend();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  randomSeed((uint32_t)esp_random());

  rn.begin(57600, SERIAL_8N1, RN_RX_PIN, RN_TX_PIN);

  Serial.println("\nROOM NODE START");

  if (!rnConfigure()) {
    Serial.println("RN2483 config failed");
    while (true) delay(1000);
  }

  scheduleNextSend();
}

void loop() {
  if ((int32_t)(millis() - nextSendAt) >= 0) {
    sendSummaryOnce();
  }
}