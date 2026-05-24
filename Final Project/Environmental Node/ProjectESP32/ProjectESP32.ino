#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE credentials
#define DEVICE_NAME         "room_node_1"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// UART from XIAO
#define RXD2 25
#define TXD2 26

// Sleep interval for testing 30 seconds, change to 60 min
#define SLEEP_SECONDS       3600
#define uS_TO_S             1000000ULL

// XIAO reply timeout
#define XIAO_TIMEOUT_MS     60000

// BLE connection timeout
#define BLE_TIMEOUT_MS      30000

BLECharacteristic* pCharacteristic;
bool hubConnected = false;
bool dataSent     = false;
String pendingReport = "";

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    hubConnected = true;
    Serial.println("Hub connected");
  }
  void onDisconnect(BLEServer* pServer) {
    hubConnected = false;
    Serial.println("Hub disconnected");
  }
};

void goToSleep() {
  Serial.println("Going to deep sleep");
  BLEDevice::deinit(true);
  delay(200);
  esp_sleep_enable_timer_wakeup(SLEEP_SECONDS * uS_TO_S);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  delay(500);
  Serial.println("Room node awake");

  // Step 1 send wake signal to XIAO
  Serial.println("Sending wake signal to XIAO");
  Serial2.println("WAKE");

  // Step 2 wait for XIAO report with timeout
  Serial.println("Waiting for XIAO report");
  unsigned long start = millis();
  while (millis() - start < XIAO_TIMEOUT_MS) {
    if (Serial2.available()) {
      String report = Serial2.readStringUntil('\n');
      report.trim();
      if (report.startsWith("N1,REPORT")) {
        pendingReport = report;
        Serial.println("Received report: " + pendingReport);
        break;
      }
    }
    delay(100);
  }

  if (pendingReport == "") {
    Serial.println("No report from XIAO going back to sleep");
    goToSleep();
  }

  // Step 3 init BLE and advertise
  BLEDevice::init(DEVICE_NAME);
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising waiting for hub");
}

void loop() {
  // Wait for hub to connect with timeout
  static unsigned long bleStart = millis();

  if (!hubConnected && millis() - bleStart > BLE_TIMEOUT_MS) {
    Serial.println("Hub did not connect going back to sleep");
    goToSleep();
  }

  // Hub connected send report
  if (hubConnected && !dataSent) {
    delay(1000);
    pCharacteristic->setValue(pendingReport.c_str());
    pCharacteristic->notify();
    Serial.println("Sent to hub: " + pendingReport);
    dataSent = true;
    delay(500);
    goToSleep();
  }

  delay(100);
}
