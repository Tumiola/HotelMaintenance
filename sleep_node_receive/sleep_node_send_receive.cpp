/*
 * CURTAIN/LIGHT NODE - ESP32
 *
 * Role: Passive node. Reports current LED and curtain status to hub via BLE.
 *       Receives commands from hub and actuates servo + LEDs accordingly.
 *
 * BLE role: SERVER (hub connects to this node as client)
 *
 * Characteristics:
 *   STATUS_CHAR   - read + notify (node -> hub): reports state as "CURTAIN:OPEN,LED:BLUE"
 *   COMMAND_CHAR  - write (hub -> node): receives commands from hub
 *
 * Commands received from hub:
 *   "CURTAIN_OPEN"  -> servo goes to open position
 *   "CURTAIN_CLOSE" -> servo goes to closed position
 *   "LED_BLUE"      -> blue LED on, red off
 *   "LED_RED"       -> red LED on, blue off
 *   "LED_OFF"       -> all LEDs off
 *
 * Wiring:
 *   Servo signal  -> GPIO 13
 *   Red LED       -> GPIO 25 (with resistor to GND)
 *   Blue LED      -> GPIO 26 (with resistor to GND)
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ESP32Servo.h>

// ─── Pin definitions ─────────────────────────────────────────────────────────
#define SERVO_PIN     13
#define LED_RED_PIN   25
#define LED_BLUE_PIN  26

// ─── Servo positions ──────────────────────────────────────────────────────────
#define SERVO_OPEN    90
#define SERVO_CLOSED  0

// ─── BLE UUIDs (must match hub script exactly) ────────────────────────────────
#define SERVICE_UUID      "12345678-1234-1234-1234-123456789abc"
#define STATUS_CHAR_UUID  "12345678-1234-1234-1234-123456789ab1"  // read + notify
#define COMMAND_CHAR_UUID "12345678-1234-1234-1234-123456789ab2"  // write

// ─── State tracking ───────────────────────────────────────────────────────────
String curtainState = "CLOSED";   // "OPEN" or "CLOSED"
String ledState     = "OFF";      // "BLUE", "RED", or "OFF"
bool deviceConnected = false;

Servo curtainServo;
BLECharacteristic* pStatusCharacteristic;

// ─── Build and send status string to hub ─────────────────────────────────────
void reportStatus() {
  String status = "CURTAIN:" + curtainState + ",LED:" + ledState;
  pStatusCharacteristic->setValue(status.c_str());
  pStatusCharacteristic->notify();
  Serial.print("[STATUS] Reported to hub: ");
  Serial.println(status);
}

// ─── BLE connection callbacks ─────────────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("[BLE] Hub connected");
    delay(100);  // small delay to let connection settle before notifying
    reportStatus();
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("[BLE] Hub disconnected - restarting advertising");
    BLEDevice::startAdvertising();
  }
};

// ─── Command received from hub ────────────────────────────────────────────────
class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String command = pCharacteristic->getValue().c_str();
    Serial.print("[CMD] Received: ");
    Serial.println(command);

    if (command == "CURTAIN_OPEN") {
      curtainServo.write(SERVO_OPEN);
      curtainState = "OPEN";
      Serial.println("[ACT] Curtain opening");

    } else if (command == "CURTAIN_CLOSE") {
      curtainServo.write(SERVO_CLOSED);
      curtainState = "CLOSED";
      Serial.println("[ACT] Curtain closing");

    } else if (command == "LED_BLUE") {
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_BLUE_PIN, HIGH);
      ledState = "BLUE";
      Serial.println("[ACT] Blue light on - morning mode");

    } else if (command == "LED_RED") {
      digitalWrite(LED_BLUE_PIN, LOW);
      digitalWrite(LED_RED_PIN, HIGH);
      ledState = "RED";
      Serial.println("[ACT] Red light on - night mode");

    } else if (command == "LED_OFF") {
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_BLUE_PIN, LOW);
      ledState = "OFF";
      Serial.println("[ACT] LEDs off");

    } else {
      Serial.println("[CMD] Unknown command ignored");
      return;
    }

    // Report updated status back to hub after every valid command
    reportStatus();
  }
};

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("[BOOT] Curtain node starting...");

  // Servo
  curtainServo.attach(SERVO_PIN, 500, 2500);
  curtainServo.write(SERVO_CLOSED);
  Serial.println("[INIT] Servo initialized - closed position");

  // LEDs
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, LOW);
  Serial.println("[INIT] LEDs initialized - off");

  // BLE
  BLEDevice::init("CurtainNode");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Status characteristic - node notifies hub of current state
  pStatusCharacteristic = pService->createCharacteristic(
    STATUS_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pStatusCharacteristic->addDescriptor(new BLE2902());
  pStatusCharacteristic->setValue("CURTAIN:CLOSED,LED:OFF");

  // Command characteristic - hub writes commands to node
  BLECharacteristic* pCommandCharacteristic = pService->createCharacteristic(
    COMMAND_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCommandCharacteristic->setCallbacks(new CommandCallbacks());

  pService->start();
  BLEDevice::startAdvertising();
  Serial.println("[BLE] Advertising as 'CurtainNode' - waiting for hub...");
}

// ─── Main loop ────────────────────────────────────────────────────────────────
void loop() {
  // Node is fully event-driven
  // All actions triggered by incoming commands from hub
  delay(100);
}
