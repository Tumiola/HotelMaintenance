#define BLYNK_TEMPLATE_ID "TMPL5X4Seqqwi"
#define BLYNK_TEMPLATE_NAME "Sleep Monitor"
#define BLYNK_AUTH_TOKEN "ruAGZXa9f8VFVLPHWFFokhgdZBWhjBv_"

#include "esp_bt.h"
#include <BLE2902.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>

// WiFi
#define WIFI_SSID ""
#define WIFI_PASS ""

// Blynk virtual pins
#define VP_TEMP V0
#define VP_HUM V1
#define VP_MOTION V2
#define VP_SOUND V3
#define VP_LIGHT_B V7
#define VP_LIGHT_R V8
#define VP_CURTAIN V9

// Shared BLE credentials for room node
#define SHARED_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SHARED_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Curtain node BLE credentials
#define CURTAIN_NODE_NAME "CurtainNode"
#define CURTAIN_SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define CURTAIN_STATUS_UUID "12345678-1234-1234-1234-123456789ab1"
#define CURTAIN_COMMAND_UUID "12345678-1234-1234-1234-123456789ab2"

// Time simulation
#define MORNING_HOUR 7
#define NIGHT_HOUR 21
#define SIM_START_HOUR 6
#define SIM_HOUR_DURATION_MS 30000

// LoRa config
#define DEVICE_ID 5
#define BASE_INTERVAL_MS 10000
#define JITTER_MS 8000
#define LORA_RST 23
#define LORA_TX 18
#define LORA_RX 19

#include <HardwareSerial.h>
HardwareSerial loraSerial(1);

// BLE state room node
bool roomNodeConnected = false;
bool roomNodeDoConnect = false;
BLEAdvertisedDevice *roomNodeDevice = nullptr;
BLERemoteCharacteristic *pRoomChar = nullptr;

// BLE state curtain node
bool curtainConnected = false;
bool curtainDoConnect = false;
BLEAdvertisedDevice *curtainDevice = nullptr;
BLERemoteCharacteristic *pCommandChar = nullptr;
BLERemoteCharacteristic *pStatusChar = nullptr;

// Global BLE clients
BLEClient *pRoomClient = nullptr;
BLEClient *pCurtainClient = nullptr;

// Curtain node state
String nodeCurtainState = "UNKNOWN";
String nodeLedState = "UNKNOWN";
String lastAppliedMode = "";

// FreeRTOS task handle
TaskHandle_t bleTaskHandle = NULL;
TaskHandle_t loraTaskHandle = NULL;

// Latest sensor data for LoRa
volatile int latestMotion = 0;
volatile int latestSound = 0;
volatile float latestTemp = 0.0;
volatile float latestHumidity = 0.0;
volatile bool newDataForLora = false;

// LoRa payload sequence counter
uint8_t loraSequence = 0;

void led_on() { digitalWrite(13, 1); }
void led_off() { digitalWrite(13, 0); }

void lora_autobaud() {
  String response = "";
  while (response == "") {
    delay(1000);
    loraSerial.write((uint8_t)0x00);
    loraSerial.write((uint8_t)0x55);
    loraSerial.println();
    loraSerial.println("sys get ver");
    response = loraSerial.readStringUntil('\n');
  }
}

// Converts a signed 16-bit value to big-endian hex.
String int16ToHex(int16_t value) {
  uint8_t highByte = (value >> 8) & 0xFF;
  uint8_t lowByte = value & 0xFF;

  char buf[5];
  sprintf(buf, "%02X%02X", highByte, lowByte);

  return String(buf);
}

// Builds backend-compatible LoRa payload.
String buildPayload(int motion, int sound, float temp, float humidity) {
  /*
    Current backend payload format:

    [DeviceID][Sequence][SensorEntryCount]
    [SensorType][Length][ValueHigh][ValueLow]
    [SensorType][Length][ValueHigh][ValueLow]

    Sensor types:
      0x01 = TEMPERATURE
      0x02 = HUMIDITY

    Motion and sound are still sent to Blynk, but not included in LoRa uplink.
  */

  uint8_t deviceId = DEVICE_ID;
  uint8_t sequence = loraSequence++;
  uint8_t sensorEntryCount = 2;

  int16_t tempRaw = (int16_t)(temp * 100);
  int16_t humidityRaw = (int16_t)(humidity * 100);

  String payload = "";

  char header[7];
  sprintf(header, "%02X%02X%02X", deviceId, sequence, sensorEntryCount);
  payload += String(header);

  // Adds temperature entry.
  payload += "01";
  payload += "02";
  payload += int16ToHex(tempRaw);

  // Adds humidity entry.
  payload += "02";
  payload += "02";
  payload += int16ToHex(humidityRaw);

  return payload;
}

int getCurrentHour() {
  int simHour = SIM_START_HOUR + (millis() / SIM_HOUR_DURATION_MS);
  return simHour % 24;
}

void parseStatus(String status) {
  Serial.print("Curtain status ");
  Serial.println(status);

  int curtainIdx = status.indexOf("CURTAIN:");
  int commaIdx = status.indexOf(",");
  if (curtainIdx != -1 && commaIdx != -1) {
    nodeCurtainState = status.substring(curtainIdx + 8, commaIdx);
  }

  int ledIdx = status.indexOf("LED:");
  if (ledIdx != -1) {
    nodeLedState = status.substring(ledIdx + 4);
  }

  Blynk.virtualWrite(VP_CURTAIN, nodeCurtainState == "OPEN" ? 1 : 0);
  Blynk.virtualWrite(VP_LIGHT_B, nodeLedState == "BLUE" ? 1 : 0);
  Blynk.virtualWrite(VP_LIGHT_R, nodeLedState == "RED" ? 1 : 0);
}

void sendCurtainCommand(String command) {
  if (!curtainConnected || pCommandChar == nullptr) {
    Serial.println("Cannot send curtain node not connected");
    return;
  }

  pCommandChar->writeValue(command.c_str(), command.length());

  Serial.print("Sent to curtain node ");
  Serial.println(command);
}

void applyMorningMode() {
  Serial.println("Applying morning mode");

  if (nodeLedState != "BLUE") {
    sendCurtainCommand("LED_BLUE");
  }

  vTaskDelay(300 / portTICK_PERIOD_MS);

  if (nodeCurtainState != "OPEN") {
    sendCurtainCommand("CURTAIN_OPEN");
  }

  lastAppliedMode = "MORNING";
}

void applyNightMode() {
  Serial.println("Applying night mode");

  if (nodeLedState != "RED") {
    sendCurtainCommand("LED_RED");
  }

  vTaskDelay(300 / portTICK_PERIOD_MS);

  if (nodeCurtainState != "CLOSED") {
    sendCurtainCommand("CURTAIN_CLOSE");
  }

  vTaskDelay(5000 / portTICK_PERIOD_MS);

  sendCurtainCommand("LED_OFF");
  lastAppliedMode = "NIGHT";
}

void routeToBlynk(String payload) {
  Serial.println("BLE IN " + payload);

  if (payload.startsWith("N1,REPORT,")) {
    String data = payload.substring(10);
    int c1 = data.indexOf(',');
    int c2 = data.indexOf(',', c1 + 1);
    int c3 = data.indexOf(',', c2 + 1);

    if (c1 > 0 && c2 > 0 && c3 > 0) {
      int motion = data.substring(0, c1).toInt();
      int sound = data.substring(c1 + 1, c2).toInt();
      float temp = data.substring(c2 + 1, c3).toFloat();
      float humidity = data.substring(c3 + 1).toFloat();

      Blynk.virtualWrite(VP_MOTION, motion);
      Blynk.virtualWrite(VP_SOUND, sound);
      Blynk.virtualWrite(VP_TEMP, temp);
      Blynk.virtualWrite(VP_HUM, humidity);

      Serial.printf("Motion: %d Sound: %d Temp: %.1f Hum: %.1f\n", motion,
                    sound, temp, humidity);

      // Stores values for LoRa uplink.
      latestMotion = motion;
      latestSound = sound;
      latestTemp = temp;
      latestHumidity = humidity;
      newDataForLora = true;
    }
  }
}

static void roomNodeCallback(BLERemoteCharacteristic *pChar, uint8_t *pData,
                             size_t length, bool isNotify) {
  String payload = "";

  for (int i = 0; i < length; i++) {
    payload += (char)pData[i];
  }

  routeToBlynk(payload);
}

static void curtainStatusCallback(BLERemoteCharacteristic *pChar,
                                  uint8_t *pData, size_t length,
                                  bool isNotify) {
  String value = "";

  for (int i = 0; i < length; i++) {
    value += (char)pData[i];
  }

  parseStatus(value);
}

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String name = advertisedDevice.getName().c_str();

    if (name == "room_node_1" && !roomNodeConnected &&
        roomNodeDevice == nullptr) {
      Serial.println("Found room node 1");
      roomNodeDevice = new BLEAdvertisedDevice(advertisedDevice);
      roomNodeDoConnect = true;
    }

    if (name == CURTAIN_NODE_NAME && !curtainConnected &&
        curtainDevice == nullptr) {
      Serial.println("Found CurtainNode");
      curtainDevice = new BLEAdvertisedDevice(advertisedDevice);
      curtainDoConnect = true;
    }
  }
};

bool connectToRoomNode() {
  Serial.println("Connecting to room node 1");

  if (pRoomClient == nullptr) {
    pRoomClient = BLEDevice::createClient();
  }

  if (pRoomClient->isConnected()) {
    pRoomClient->disconnect();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }

  pRoomClient->connect(roomNodeDevice);

  unsigned long start = millis();
  while (!pRoomClient->isConnected() && millis() - start < 5000) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  if (!pRoomClient->isConnected()) {
    Serial.println("room node 1 connection timed out");
    pRoomClient->disconnect();
    return false;
  }

  BLERemoteService *pService =
      pRoomClient->getService(BLEUUID(SHARED_SERVICE_UUID));
  if (!pService) {
    pRoomClient->disconnect();
    return false;
  }

  pRoomChar = pService->getCharacteristic(BLEUUID(SHARED_CHAR_UUID));
  if (!pRoomChar) {
    pRoomClient->disconnect();
    return false;
  }

  if (pRoomChar->canNotify()) {
    pRoomChar->registerForNotify(roomNodeCallback);
  }

  roomNodeConnected = true;
  Serial.println("Connected to room node 1");

  return true;
}

bool connectToCurtainNode() {
  Serial.println("Connecting to CurtainNode");

  if (pCurtainClient == nullptr) {
    pCurtainClient = BLEDevice::createClient();
  }

  if (pCurtainClient->isConnected()) {
    pCurtainClient->disconnect();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }

  pCurtainClient->connect(curtainDevice);

  unsigned long start = millis();
  while (!pCurtainClient->isConnected() && millis() - start < 5000) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  if (!pCurtainClient->isConnected()) {
    Serial.println("CurtainNode connection timed out");
    pCurtainClient->disconnect();
    return false;
  }

  BLERemoteService *pService =
      pCurtainClient->getService(BLEUUID(CURTAIN_SERVICE_UUID));
  if (!pService) {
    pCurtainClient->disconnect();
    return false;
  }

  pCommandChar = pService->getCharacteristic(BLEUUID(CURTAIN_COMMAND_UUID));
  if (!pCommandChar) {
    pCurtainClient->disconnect();
    return false;
  }

  pStatusChar = pService->getCharacteristic(BLEUUID(CURTAIN_STATUS_UUID));
  if (!pStatusChar) {
    pCurtainClient->disconnect();
    return false;
  }

  if (pStatusChar->canNotify()) {
    pStatusChar->registerForNotify(curtainStatusCallback);
  }

  String initialStatus = pStatusChar->readValue().c_str();
  parseStatus(initialStatus);

  curtainConnected = true;
  Serial.println("Connected to CurtainNode");

  return true;
}

BLYNK_WRITE(VP_LIGHT_B) {
  int val = param.asInt();

  Serial.printf("Blynk light blue %d\n", val);

  sendCurtainCommand(val == 1 ? "LED_BLUE" : "LED_OFF");
  lastAppliedMode = "MANUAL";
}

BLYNK_WRITE(VP_LIGHT_R) {
  int val = param.asInt();

  Serial.printf("Blynk light red %d\n", val);

  sendCurtainCommand(val == 1 ? "LED_RED" : "LED_OFF");
  lastAppliedMode = "MANUAL";
}

BLYNK_WRITE(VP_CURTAIN) {
  int val = param.asInt();

  Serial.printf("Blynk curtain %d\n", val);

  sendCurtainCommand(val == 1 ? "CURTAIN_OPEN" : "CURTAIN_CLOSE");
  lastAppliedMode = "MANUAL";
}

void handleLoraRx(String hex) {
  hex.trim();
  hex.toUpperCase();

  if (hex.length() < 6) {
    Serial.println("[LORA RX] Payload too short, ignoring");
    return;
  }

  uint8_t rxDeviceId =
      (uint8_t)strtol(hex.substring(0, 2).c_str(), nullptr, 16);

  if (rxDeviceId != DEVICE_ID) {
    Serial.printf("[LORA RX] Not for us (got %02X, ours %02X)\n", rxDeviceId,
                  DEVICE_ID);
    return;
  }

  uint8_t service = (uint8_t)strtol(hex.substring(2, 4).c_str(), nullptr, 16);
  uint8_t value = (uint8_t)strtol(hex.substring(4, 6).c_str(), nullptr, 16);

  Serial.printf("[LORA RX] Device: %02X  Service: %02X  Value: %02X\n",
                rxDeviceId, service, value);

  switch (service) {
  case 0x03:
    if (value == 0x01) {
      Serial.println("[LORA RX] CMD: CURTAIN_OPEN");
      sendCurtainCommand("CURTAIN_OPEN");
    } else {
      Serial.println("[LORA RX] CMD: CURTAIN_CLOSE");
      sendCurtainCommand("CURTAIN_CLOSE");
    }

    lastAppliedMode = "MANUAL";
    break;

  case 0x04:
    if (value == 0x01) {
      Serial.println("[LORA RX] CMD: LED_BLUE");
      sendCurtainCommand("LED_BLUE");
    } else if (value == 0x02) {
      Serial.println("[LORA RX] CMD: LED_RED");
      sendCurtainCommand("LED_RED");
    } else {
      Serial.println("[LORA RX] CMD: LED_OFF");
      sendCurtainCommand("LED_OFF");
    }

    lastAppliedMode = "MANUAL";
    break;

  case 0x05:
    if (value == 0x01) {
      Serial.println("[LORA RX] CMD: Force morning mode");
      applyMorningMode();
    } else if (value == 0x02) {
      Serial.println("[LORA RX] CMD: Force night mode");
      applyNightMode();
    }

    break;

  default:
    Serial.printf("[LORA RX] Unknown service: 0x%02X\n", service);
    break;
  }
}

void loraTask(void *parameter) {
  String str;

  pinMode(13, OUTPUT);
  led_off();

  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);

  loraSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  loraSerial.setTimeout(60000);

  digitalWrite(LORA_RST, LOW);
  delay(200);
  digitalWrite(LORA_RST, HIGH);
  delay(200);

  lora_autobaud();

  led_on();
  delay(1000);
  led_off();

  Serial.println("Initing LoRa");

  loraSerial.read();
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("sys get ver");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("mac pause");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set mod lora");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set freq 869100000");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set pwr 14");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set sf sf7");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set afcbw 41.7");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set rxbw 125");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set prlen 8");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set crc on");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set iqi off");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set cr 4/5");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set wdt 60000");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set sync 12");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set bw 125");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  randomSeed(esp_random());

  Serial.println("LoRa ready");

  while (true) {
    if (newDataForLora) {
      led_on();

      String payload =
          buildPayload(latestMotion, latestSound, latestTemp, latestHumidity);

      Serial.printf("[LORA TX] Device:%02d Payload:%s\n", DEVICE_ID,
                    payload.c_str());

      loraSerial.println("radio tx " + payload);

      str = loraSerial.readStringUntil('\n');
      Serial.println(str);

      str = loraSerial.readStringUntil('\n');
      Serial.println(str);

      led_off();

      newDataForLora = false;

      uint32_t waitMs = BASE_INTERVAL_MS + random(0, JITTER_MS);

      Serial.printf("[LORA TX] Next LoRa TX in %lums\n", waitMs);

      vTaskDelay(waitMs / portTICK_PERIOD_MS);
    } else {
      loraSerial.println("radio rx 5000");

      str = loraSerial.readStringUntil('\n');

      if (str.indexOf("ok") >= 0) {
        str = loraSerial.readStringUntil('\n');

        if (str.startsWith("radio_rx")) {
          String rxPayload = str.substring(8);
          rxPayload.trim();

          Serial.println("[LORA RX] " + rxPayload);

          handleLoraRx(rxPayload);
        }
      }

      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}

void bleTask(void *parameter) {
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  BLEDevice::init("RoomHub");
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N12);

  BLEScan *pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new ScanCallbacks());
  pScan->setActiveScan(true);

  Serial.println("BLE scanning for nodes");

  pScan->start(10, false);

  while (true) {
    if (roomNodeDoConnect && !roomNodeConnected) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);

      if (!connectToRoomNode()) {
        roomNodeDevice = nullptr;
      }

      roomNodeDoConnect = false;
    }

    if (curtainDoConnect && !curtainConnected) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);

      if (!connectToCurtainNode()) {
        curtainDevice = nullptr;
      }

      curtainDoConnect = false;
    }

    if (pRoomClient != nullptr && !pRoomClient->isConnected() &&
        roomNodeConnected) {
      Serial.println("Room node disconnected resetting for next wake cycle");

      roomNodeConnected = false;
      roomNodeDevice = nullptr;
      pRoomChar = nullptr;
    }

    static unsigned long lastScan = 0;
    bool anyDisconnected = !roomNodeConnected || !curtainConnected;

    if (anyDisconnected && millis() - lastScan > 10000) {
      Serial.println("Rescanning for missing nodes");

      BLEDevice::getScan()->start(5, false);

      lastScan = millis();
    }

    if (curtainConnected) {
      int hour = getCurrentHour();

      static int lastLoggedHour = -1;

      if (hour != lastLoggedHour) {
        Serial.printf("Simulated hour %d\n", hour);
        lastLoggedHour = hour;
      }

      if (hour == MORNING_HOUR && lastAppliedMode != "MORNING") {
        applyMorningMode();
      } else if (hour == NIGHT_HOUR && lastAppliedMode != "NIGHT") {
        applyNightMode();
      }
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("Room hub starting");

  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);

  Serial.println("Blynk connected");

  xTaskCreatePinnedToCore(bleTask, "BLE Task", 30000, NULL, 1, &bleTaskHandle,
                          0);

  xTaskCreatePinnedToCore(loraTask, "LoRa Task", 16000, NULL, 1,
                          &loraTaskHandle, 1);
}

void loop() {
  Blynk.run();
  delay(10);
}