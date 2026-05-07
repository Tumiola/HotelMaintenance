#define BLYNK_TEMPLATE_ID   "TMPL5X4Seqqwi"
#define BLYNK_TEMPLATE_NAME "Sleep Monitor"
#define BLYNK_AUTH_TOKEN    "ruAGZXa9f8VFVLPHWFFokhgdZBWhjBv_"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include "esp_bt.h"

// WiFi
#define WIFI_SSID "Emre iPhone’u"
#define WIFI_PASS "emre1234"

// Blynk virtual pins
#define VP_TEMP     V0
#define VP_HUM      V1
#define VP_MOTION   V2
#define VP_SOUND    V3
#define VP_LIGHT_B  V7
#define VP_LIGHT_R  V8
#define VP_CURTAIN  V9

// Shared BLE credentials for room node
#define SHARED_SERVICE_UUID   "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SHARED_CHAR_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Curtain node BLE credentials
#define CURTAIN_NODE_NAME     "CurtainNode"
#define CURTAIN_SERVICE_UUID  "12345678-1234-1234-1234-123456789abc"
#define CURTAIN_STATUS_UUID   "12345678-1234-1234-1234-123456789ab1"
#define CURTAIN_COMMAND_UUID  "12345678-1234-1234-1234-123456789ab2"

// Time simulation
#define MORNING_HOUR          7
#define NIGHT_HOUR            21
#define SIM_START_HOUR        6
#define SIM_HOUR_DURATION_MS  30000

// BLE state room node
bool roomNodeConnected  = false;
bool roomNodeDoConnect  = false;
BLEAdvertisedDevice* roomNodeDevice = nullptr;
BLERemoteCharacteristic* pRoomChar  = nullptr;

// BLE state curtain node
bool curtainConnected   = false;
bool curtainDoConnect   = false;
BLEAdvertisedDevice* curtainDevice  = nullptr;
BLERemoteCharacteristic* pCommandChar = nullptr;
BLERemoteCharacteristic* pStatusChar  = nullptr;

// Global BLE clients
BLEClient* pRoomClient    = nullptr;
BLEClient* pCurtainClient = nullptr;

// Curtain node state
String nodeCurtainState = "UNKNOWN";
String nodeLedState     = "UNKNOWN";
String lastAppliedMode  = "";

// FreeRTOS task handle
TaskHandle_t bleTaskHandle = NULL;

int getCurrentHour() {
  int simHour = SIM_START_HOUR + (millis() / SIM_HOUR_DURATION_MS);
  return simHour % 24;
}

void parseStatus(String status) {
  Serial.print("Curtain status ");
  Serial.println(status);

  int curtainIdx = status.indexOf("CURTAIN:");
  int commaIdx   = status.indexOf(",");
  if (curtainIdx != -1 && commaIdx != -1) {
    nodeCurtainState = status.substring(curtainIdx + 8, commaIdx);
  }

  int ledIdx = status.indexOf("LED:");
  if (ledIdx != -1) {
    nodeLedState = status.substring(ledIdx + 4);
  }

  Blynk.virtualWrite(VP_CURTAIN, nodeCurtainState == "OPEN" ? 1 : 0);
  Blynk.virtualWrite(VP_LIGHT_B, nodeLedState == "BLUE" ? 1 : 0);
  Blynk.virtualWrite(VP_LIGHT_R, nodeLedState == "RED"  ? 1 : 0);
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
  if (nodeLedState != "BLUE")     sendCurtainCommand("LED_BLUE");
  vTaskDelay(300 / portTICK_PERIOD_MS);
  if (nodeCurtainState != "OPEN") sendCurtainCommand("CURTAIN_OPEN");
  lastAppliedMode = "MORNING";
}

void applyNightMode() {
  Serial.println("Applying night mode");
  if (nodeLedState != "RED")        sendCurtainCommand("LED_RED");
  vTaskDelay(300 / portTICK_PERIOD_MS);
  if (nodeCurtainState != "CLOSED") sendCurtainCommand("CURTAIN_CLOSE");
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  sendCurtainCommand("LED_OFF");
  lastAppliedMode = "NIGHT";
}

void routeToBlynk(String payload) {
  Serial.println("BLE IN " + payload);

  // Parse N1,REPORT,motionCount,soundCount,temp,humidity
  if (payload.startsWith("N1,REPORT,")) {
    String data = payload.substring(10);
    int c1 = data.indexOf(',');
    int c2 = data.indexOf(',', c1 + 1);
    int c3 = data.indexOf(',', c2 + 1);

    if (c1 > 0 && c2 > 0 && c3 > 0) {
      int   motion   = data.substring(0, c1).toInt();
      int   sound    = data.substring(c1 + 1, c2).toInt();
      float temp     = data.substring(c2 + 1, c3).toFloat();
      float humidity = data.substring(c3 + 1).toFloat();

      Blynk.virtualWrite(VP_MOTION, motion);
      Blynk.virtualWrite(VP_SOUND,  sound);
      Blynk.virtualWrite(VP_TEMP,   temp);
      Blynk.virtualWrite(VP_HUM,    humidity);

      Serial.printf("Motion: %d Sound: %d Temp: %.1f Hum: %.1f\n",
                    motion, sound, temp, humidity);
    }
  }
}

static void roomNodeCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  String payload = "";
  for (int i = 0; i < length; i++) payload += (char)pData[i];
  routeToBlynk(payload);
}

static void curtainStatusCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  String value = "";
  for (int i = 0; i < length; i++) value += (char)pData[i];
  parseStatus(value);
}

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String name = advertisedDevice.getName().c_str();

    if (name == "room_node_1" && !roomNodeConnected && roomNodeDevice == nullptr) {
      Serial.println("Found room node 1");
      roomNodeDevice = new BLEAdvertisedDevice(advertisedDevice);
      roomNodeDoConnect = true;
    }

    if (name == CURTAIN_NODE_NAME && !curtainConnected && curtainDevice == nullptr) {
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

  BLERemoteService* pService = pRoomClient->getService(BLEUUID(SHARED_SERVICE_UUID));
  if (!pService) { pRoomClient->disconnect(); return false; }

  pRoomChar = pService->getCharacteristic(BLEUUID(SHARED_CHAR_UUID));
  if (!pRoomChar) { pRoomClient->disconnect(); return false; }

  if (pRoomChar->canNotify()) pRoomChar->registerForNotify(roomNodeCallback);

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

  BLERemoteService* pService = pCurtainClient->getService(BLEUUID(CURTAIN_SERVICE_UUID));
  if (!pService) { pCurtainClient->disconnect(); return false; }

  pCommandChar = pService->getCharacteristic(BLEUUID(CURTAIN_COMMAND_UUID));
  if (!pCommandChar) { pCurtainClient->disconnect(); return false; }

  pStatusChar = pService->getCharacteristic(BLEUUID(CURTAIN_STATUS_UUID));
  if (!pStatusChar) { pCurtainClient->disconnect(); return false; }

  if (pStatusChar->canNotify()) pStatusChar->registerForNotify(curtainStatusCallback);

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

void bleTask(void* parameter) {
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  BLEDevice::init("RoomHub");
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N12);

  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new ScanCallbacks());
  pScan->setActiveScan(true);
  Serial.println("BLE scanning for nodes");
  pScan->start(10, false);

  while (true) {
    if (roomNodeDoConnect && !roomNodeConnected) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      if (!connectToRoomNode()) roomNodeDevice = nullptr;
      roomNodeDoConnect = false;
    }

    if (curtainDoConnect && !curtainConnected) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      if (!connectToCurtainNode()) curtainDevice = nullptr;
      curtainDoConnect = false;
    }

    // Room node disconnects every sleep cycle so reset state when it drops
    if (pRoomClient != nullptr && !pRoomClient->isConnected() && roomNodeConnected) {
      Serial.println("Room node disconnected resetting for next wake cycle");
      roomNodeConnected = false;
      roomNodeDevice    = nullptr;
      pRoomChar         = nullptr;
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

  xTaskCreatePinnedToCore(
    bleTask,
    "BLE Task",
    30000,
    NULL,
    1,
    &bleTaskHandle,
    0
  );
}

void loop() {
  Blynk.run();
  delay(10);
}