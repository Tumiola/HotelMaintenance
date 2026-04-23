#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

/*
  ROOM NODE (ESP32 DevKit V1) - Combined BLE Receiver

  Purpose:
  - Receive environmental telemetry from DHT/motion/sound node.
  - Receive curtain/light status from circadian actuator node.
  - Keep both connections active and print a single merged stream to Serial.

  Notes:
  - Update the names/UUIDs below if your sender nodes use different values.
  - This sketch is receive-only (no actuator command writes).
*/

// -------------------- Environmental node identifiers --------------------
// Based on your existing DHT11_receive setup
static BLEUUID ENV_SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID ENV_CHAR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
const char* ENV_NODE_NAME = "XIAO_ENV";  // Optional filter; can be ignored if service UUID matches

// -------------------- Light/Curtain node identifiers --------------------
// Based on your existing light_send setup
static BLEUUID LIGHT_SERVICE_UUID("12345678-1234-1234-1234-123456789abc");
static BLEUUID LIGHT_STATUS_CHAR_UUID("12345678-1234-1234-1234-123456789ab1");
const char* LIGHT_NODE_NAME = "CurtainNode";

// -------------------- Runtime state --------------------
BLEAdvertisedDevice* envDevice = nullptr;
BLEAdvertisedDevice* lightDevice = nullptr;

BLEClient* envClient = nullptr;
BLEClient* lightClient = nullptr;

bool envConnected = false;
bool lightConnected = false;

// Latest environmental values
float roomTemp = NAN;
float roomHum = NAN;
int soundLevel = -1;
int motionIndex = -1;

// Latest light/curtain status
String curtainState = "UNKNOWN";
String ledState = "UNKNOWN";

unsigned long lastMergedPrintMs = 0;
const unsigned long MERGED_PRINT_INTERVAL_MS = 15000;

// -------------------- Windowing policy --------------------
// ENV data is not clinically useful at sub-second granularity for this use case.
// We accept ENV packets only every ENV_ACCEPT_WINDOW_MS, unless values changed
// meaningfully (thresholds below).
const unsigned long ENV_ACCEPT_WINDOW_MS = 15000;   // 15 s
const float TEMP_CHANGE_THRESHOLD_C = 0.3f;         // deg C
const float HUM_CHANGE_THRESHOLD_PCT = 2.0f;        // %
const int SOUND_CHANGE_THRESHOLD = 8;               // ADC-like units
const int MOTION_CHANGE_THRESHOLD = 8;              // index units

// Always accept first LIGHT status, then on-change, plus periodic heartbeat.
const unsigned long LIGHT_HEARTBEAT_WINDOW_MS = 30000;  // 30 s

unsigned long lastEnvAcceptedMs = 0;
unsigned long lastLightAcceptedMs = 0;
bool haveEnvBaseline = false;
bool haveLightBaseline = false;

float lastAcceptedTemp = NAN;
float lastAcceptedHum = NAN;
int lastAcceptedSound = -1;
int lastAcceptedMotion = -1;
String lastAcceptedCurtain = "UNKNOWN";
String lastAcceptedLed = "UNKNOWN";

// -------------------- Parsing helpers --------------------
void parseEnvPayload(const String& payload) {
  // Expected CSV: "Temp,Hum,Sound,Motion"
  int firstComma = payload.indexOf(',');
  int secondComma = payload.indexOf(',', firstComma + 1);
  int thirdComma = payload.indexOf(',', secondComma + 1);

  if (firstComma > 0 && secondComma > 0 && thirdComma > 0) {
    roomTemp = payload.substring(0, firstComma).toFloat();
    roomHum = payload.substring(firstComma + 1, secondComma).toFloat();
    soundLevel = payload.substring(secondComma + 1, thirdComma).toInt();
    motionIndex = payload.substring(thirdComma + 1).toInt();
  } else {
    Serial.print("[ENV] Invalid payload format: ");
    Serial.println(payload);
  }
}

void parseLightStatus(const String& payload) {
  // Expected status: "CURTAIN:OPEN,LED:BLUE"
  int curtainIdx = payload.indexOf("CURTAIN:");
  int commaIdx = payload.indexOf(',');
  int ledIdx = payload.indexOf("LED:");

  if (curtainIdx != -1 && commaIdx != -1) {
    curtainState = payload.substring(curtainIdx + 8, commaIdx);
  }
  if (ledIdx != -1) {
    ledState = payload.substring(ledIdx + 4);
  }
}

bool shouldAcceptEnvUpdate(float temp, float hum, int sound, int motion) {
  unsigned long now = millis();
  if (!haveEnvBaseline) return true;

  bool windowElapsed = (now - lastEnvAcceptedMs) >= ENV_ACCEPT_WINDOW_MS;
  bool tempChanged = fabs(temp - lastAcceptedTemp) >= TEMP_CHANGE_THRESHOLD_C;
  bool humChanged = fabs(hum - lastAcceptedHum) >= HUM_CHANGE_THRESHOLD_PCT;
  bool soundChanged = abs(sound - lastAcceptedSound) >= SOUND_CHANGE_THRESHOLD;
  bool motionChanged = abs(motion - lastAcceptedMotion) >= MOTION_CHANGE_THRESHOLD;

  return windowElapsed || tempChanged || humChanged || soundChanged || motionChanged;
}

bool shouldAcceptLightUpdate(const String& curtain, const String& led) {
  unsigned long now = millis();
  if (!haveLightBaseline) return true;

  bool heartbeatElapsed = (now - lastLightAcceptedMs) >= LIGHT_HEARTBEAT_WINDOW_MS;
  bool stateChanged = (curtain != lastAcceptedCurtain) || (led != lastAcceptedLed);

  return heartbeatElapsed || stateChanged;
}

void printMergedStatus() {
  Serial.println("==========================================");
  Serial.println("[ROOM NODE] Combined Snapshot");

  Serial.print("ENV  | Temp: ");
  if (isnan(roomTemp)) Serial.print("NA");
  else Serial.print(roomTemp, 1);
  Serial.print(" C, Hum: ");
  if (isnan(roomHum)) Serial.print("NA");
  else Serial.print(roomHum, 1);
  Serial.print(" %, Sound: ");
  if (soundLevel < 0) Serial.print("NA");
  else Serial.print(soundLevel);
  Serial.print(", Motion: ");
  if (motionIndex < 0) Serial.println("NA");
  else Serial.println(motionIndex);

  Serial.print("LIGHT| Curtain: ");
  Serial.print(curtainState);
  Serial.print(", LED: ");
  Serial.println(ledState);
  Serial.println("==========================================");
}

// -------------------- Notify callbacks --------------------
static void envNotifyCallback(
  BLERemoteCharacteristic* pChar,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  String payload;
  payload.reserve(length);
  for (size_t i = 0; i < length; i++) payload += (char)pData[i];

  // Parse into temporary values first (windowing decision before commit).
  int firstComma = payload.indexOf(',');
  int secondComma = payload.indexOf(',', firstComma + 1);
  int thirdComma = payload.indexOf(',', secondComma + 1);
  if (!(firstComma > 0 && secondComma > 0 && thirdComma > 0)) {
    Serial.print("[ENV] Invalid payload format: ");
    Serial.println(payload);
    return;
  }

  float newTemp = payload.substring(0, firstComma).toFloat();
  float newHum = payload.substring(firstComma + 1, secondComma).toFloat();
  int newSound = payload.substring(secondComma + 1, thirdComma).toInt();
  int newMotion = payload.substring(thirdComma + 1).toInt();

  if (!shouldAcceptEnvUpdate(newTemp, newHum, newSound, newMotion)) {
    return;
  }

  roomTemp = newTemp;
  roomHum = newHum;
  soundLevel = newSound;
  motionIndex = newMotion;

  lastAcceptedTemp = newTemp;
  lastAcceptedHum = newHum;
  lastAcceptedSound = newSound;
  lastAcceptedMotion = newMotion;
  lastEnvAcceptedMs = millis();
  haveEnvBaseline = true;

  Serial.print("[ENV] Accepted: ");
  Serial.println(payload);
}

static void lightNotifyCallback(
  BLERemoteCharacteristic* pChar,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  String payload;
  payload.reserve(length);
  for (size_t i = 0; i < length; i++) payload += (char)pData[i];

  // Parse into temporary values first
  String tmpCurtain = curtainState;
  String tmpLed = ledState;

  int curtainIdx = payload.indexOf("CURTAIN:");
  int commaIdx = payload.indexOf(',');
  int ledIdx = payload.indexOf("LED:");

  if (curtainIdx != -1 && commaIdx != -1) {
    tmpCurtain = payload.substring(curtainIdx + 8, commaIdx);
  }
  if (ledIdx != -1) {
    tmpLed = payload.substring(ledIdx + 4);
  }

  if (!shouldAcceptLightUpdate(tmpCurtain, tmpLed)) {
    return;
  }

  parseLightStatus(payload);
  lastAcceptedCurtain = curtainState;
  lastAcceptedLed = ledState;
  lastLightAcceptedMs = millis();
  haveLightBaseline = true;

  Serial.print("[LIGHT] Accepted: ");
  Serial.println(payload);
}

// -------------------- Scanner callback --------------------
class CombinedAdvertisedCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    bool haveName = advertisedDevice.haveName();
    String name = haveName ? advertisedDevice.getName().c_str() : "";

    if (!envDevice && advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(ENV_SERVICE_UUID)) {
      envDevice = new BLEAdvertisedDevice(advertisedDevice);
      Serial.print("[SCAN] Found ENV node by service UUID");
      if (haveName) {
        Serial.print(" (");
        Serial.print(name);
        Serial.print(")");
      }
      Serial.println();
    }

    if (!lightDevice && (
        (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(LIGHT_SERVICE_UUID)) ||
        (haveName && name == LIGHT_NODE_NAME))) {
      lightDevice = new BLEAdvertisedDevice(advertisedDevice);
      Serial.print("[SCAN] Found LIGHT node");
      if (haveName) {
        Serial.print(" (");
        Serial.print(name);
        Serial.print(")");
      }
      Serial.println();
    }

    // Stop early when both nodes are discovered
    if (envDevice && lightDevice) {
      BLEDevice::getScan()->stop();
    }
  }
};

// -------------------- Connection helpers --------------------
bool connectEnvNode() {
  if (!envDevice) return false;
  if (envConnected) return true;

  Serial.println("[ENV] Connecting...");
  envClient = BLEDevice::createClient();
  if (!envClient->connect(envDevice)) {
    Serial.println("[ENV] Connect failed");
    return false;
  }

  BLERemoteService* service = envClient->getService(ENV_SERVICE_UUID);
  if (!service) {
    Serial.println("[ENV] Service not found");
    envClient->disconnect();
    return false;
  }

  BLERemoteCharacteristic* ch = service->getCharacteristic(ENV_CHAR_UUID);
  if (!ch) {
    Serial.println("[ENV] Characteristic not found");
    envClient->disconnect();
    return false;
  }

  if (ch->canNotify()) {
    ch->registerForNotify(envNotifyCallback);
    Serial.println("[ENV] Notify subscribed");
  } else if (ch->canRead()) {
    // Fallback for senders that only expose read
    String initial = ch->readValue().c_str();
    parseEnvPayload(initial);
    lastAcceptedTemp = roomTemp;
    lastAcceptedHum = roomHum;
    lastAcceptedSound = soundLevel;
    lastAcceptedMotion = motionIndex;
    lastEnvAcceptedMs = millis();
    haveEnvBaseline = true;
    Serial.println("[ENV] Read initial payload");
  }

  envConnected = true;
  Serial.println("[ENV] Connected");
  return true;
}

bool connectLightNode() {
  if (!lightDevice) return false;
  if (lightConnected) return true;

  Serial.println("[LIGHT] Connecting...");
  lightClient = BLEDevice::createClient();
  if (!lightClient->connect(lightDevice)) {
    Serial.println("[LIGHT] Connect failed");
    return false;
  }

  BLERemoteService* service = lightClient->getService(LIGHT_SERVICE_UUID);
  if (!service) {
    Serial.println("[LIGHT] Service not found");
    lightClient->disconnect();
    return false;
  }

  BLERemoteCharacteristic* statusCh = service->getCharacteristic(LIGHT_STATUS_CHAR_UUID);
  if (!statusCh) {
    Serial.println("[LIGHT] Status characteristic not found");
    lightClient->disconnect();
    return false;
  }

  if (statusCh->canNotify()) {
    statusCh->registerForNotify(lightNotifyCallback);
    Serial.println("[LIGHT] Notify subscribed");
  }

  if (statusCh->canRead()) {
    String initial = statusCh->readValue().c_str();
    parseLightStatus(initial);
    lastAcceptedCurtain = curtainState;
    lastAcceptedLed = ledState;
    lastLightAcceptedMs = millis();
    haveLightBaseline = true;
    Serial.print("[LIGHT] Initial status: ");
    Serial.println(initial);
  }

  lightConnected = true;
  Serial.println("[LIGHT] Connected");
  return true;
}

void scanForNodes() {
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new CombinedAdvertisedCallbacks());
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);

  Serial.println("[SCAN] Scanning for ENV + LIGHT nodes...");
  scan->start(5, false);
}

void reconnectIfNeeded() {
  if (envConnected && envClient && !envClient->isConnected()) {
    envConnected = false;
    Serial.println("[ENV] Disconnected");
  }
  if (lightConnected && lightClient && !lightClient->isConnected()) {
    lightConnected = false;
    Serial.println("[LIGHT] Disconnected");
  }

  if (!envConnected || !lightConnected) {
    scanForNodes();
    if (!envConnected) connectEnvNode();
    if (!lightConnected) connectLightNode();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("[BOOT] Combined Room Receiver starting...");
  Serial.println("[INFO] Waiting for both sender nodes...");

  BLEDevice::init("ROOM_COMBINED_HEAD");
  scanForNodes();

  connectEnvNode();
  connectLightNode();
}

void loop() {
  reconnectIfNeeded();

  if (millis() - lastMergedPrintMs > MERGED_PRINT_INTERVAL_MS) {
    printMergedStatus();
    lastMergedPrintMs = millis();
  }

  delay(150);
}

