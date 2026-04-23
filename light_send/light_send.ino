/*
* ROOM HUB - ESP32
*
* Role: Central hub. Connects to CurtainNode via BLE.
*       On connect, reads node status. At scheduled times, checks state
*       and sends only the commands needed to reach the target mode.
*
* BLE role: CLIENT (scans for and connects to CurtainNode)
*
* Time logic (hardcoded for now, replace with NTP/RTC later):
*   MORNING_HOUR = 7  -> LED_BLUE, then CURTAIN_OPEN if closed
*   NIGHT_HOUR   = 21 -> LED_RED, then CURTAIN_CLOSE, then LED_OFF after delay
*
* Simulated time: starts at 06:00 and advances 1 hour every 30 seconds
* so you can watch the full morning/night cycle without waiting.
* Change SIMULATE_TIME to false and set currentHour manually to test
* a specific time, or replace with real NTP time later.
*
* Manual Serial Monitor commands for testing:
*   Type "CURTAIN_OPEN", "CURTAIN_CLOSE", "LED_BLUE", "LED_RED", "LED_OFF"
*/


#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>


// ─── BLE UUIDs (must match node script exactly) ───────────────────────────────
#define SERVICE_UUID      "12345678-1234-1234-1234-123456789abc"
#define STATUS_CHAR_UUID  "12345678-1234-1234-1234-123456789ab1"
#define COMMAND_CHAR_UUID "12345678-1234-1234-1234-123456789ab2"


// ─── Time settings ────────────────────────────────────────────────────────────
#define MORNING_HOUR  7
#define NIGHT_HOUR    21


// ─── Simulation settings ──────────────────────────────────────────────────────
#define SIMULATE_TIME true          // set false to use fixed currentHour below
#define SIM_START_HOUR 6            // simulation starts at 06:00
#define SIM_HOUR_DURATION_MS 30000  // each simulated hour lasts 30 seconds


// ─── Globals ──────────────────────────────────────────────────────────────────
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pCommandChar = nullptr;
BLERemoteCharacteristic* pStatusChar  = nullptr;


bool connected   = false;
bool doConnect   = false;
BLEAdvertisedDevice* targetDevice = nullptr;


// Tracks last mode applied so we don't repeat commands every loop
String lastAppliedMode = "";


// Node's last known state (parsed from status notifications)
String nodeCurtainState = "UNKNOWN";
String nodeLedState     = "UNKNOWN";


// ─── Get simulated or fixed current hour ──────────────────────────────────────
int getCurrentHour() {
 if (SIMULATE_TIME) {
   int simHour = SIM_START_HOUR + (millis() / SIM_HOUR_DURATION_MS);
   return simHour % 24;
 }
 // TODO: replace with NTP or RTC time when available
 // For now return a fixed hour for testing - change this value manually
 return 7;  // <-- change this to test different times
}


// ─── Parse status string from node e.g. "CURTAIN:OPEN,LED:BLUE" ──────────────
void parseStatus(String status) {
 Serial.print("[STATUS] Node reports: ");
 Serial.println(status);


 // Extract curtain state
 int curtainIdx = status.indexOf("CURTAIN:");
 int commaIdx   = status.indexOf(",");
 if (curtainIdx != -1 && commaIdx != -1) {
   nodeCurtainState = status.substring(curtainIdx + 8, commaIdx);
 }


 // Extract LED state
 int ledIdx = status.indexOf("LED:");
 if (ledIdx != -1) {
   nodeLedState = status.substring(ledIdx + 4);
 }


 Serial.print("[STATUS] Parsed -> Curtain: ");
 Serial.print(nodeCurtainState);
 Serial.print(" | LED: ");
 Serial.println(nodeLedState);
}


// ─── Send a command to the node ───────────────────────────────────────────────
void sendCommand(String command) {
 if (!connected || pCommandChar == nullptr) {
   Serial.println("[HUB] Cannot send - not connected");
   return;
 }
 pCommandChar->writeValue(command.c_str(), command.length());
 Serial.print("[HUB] Sent command: ");
 Serial.println(command);
}


// ─── Apply morning mode - only send commands that are actually needed ─────────
void applyMorningMode() {
 Serial.println("[HUB] Applying morning mode");


 // Turn on blue light if not already on
 if (nodeLedState != "BLUE") {
   sendCommand("LED_BLUE");
   delay(300);
 } else {
   Serial.println("[HUB] Blue light already on - skipping");
 }


 // Open curtain if not already open
 if (nodeCurtainState != "OPEN") {
   sendCommand("CURTAIN_OPEN");
 } else {
   Serial.println("[HUB] Curtain already open - skipping");
 }


 lastAppliedMode = "MORNING";
}


// ─── Apply night mode - only send commands that are actually needed ───────────
void applyNightMode() {
 Serial.println("[HUB] Applying night mode");


 // Turn on red light if not already on
 if (nodeLedState != "RED") {
   sendCommand("LED_RED");
   delay(300);
 } else {
   Serial.println("[HUB] Red light already on - skipping");
 }


 // Close curtain if not already closed
 if (nodeCurtainState != "CLOSED") {
   sendCommand("CURTAIN_CLOSE");
   delay(300);
 } else {
   Serial.println("[HUB] Curtain already closed - skipping");
 }


 // Turn off red light after 5 seconds
 Serial.println("[HUB] Red light will turn off in 5 seconds...");
 delay(5000);
 sendCommand("LED_OFF");


 lastAppliedMode = "NIGHT";
}


// ─── Status notification callback - fires when node sends a status update ─────
static void statusNotifyCallback(
 BLERemoteCharacteristic* pChar,
 uint8_t* pData,
 size_t length,
 bool isNotify
) {
 String value = "";
 for (int i = 0; i < length; i++) {
   value += (char)pData[i];
 }
 parseStatus(value);
}


// ─── BLE scan callback ────────────────────────────────────────────────────────
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
 void onResult(BLEAdvertisedDevice advertisedDevice) {
   if (advertisedDevice.getName() == "CurtainNode") {
     Serial.println("[SCAN] CurtainNode found");
     BLEDevice::getScan()->stop();
     targetDevice = new BLEAdvertisedDevice(advertisedDevice);
     doConnect = true;
   }
 }
};


// ─── Connect to node ──────────────────────────────────────────────────────────
bool connectToNode() {
 Serial.println("[BLE] Connecting to CurtainNode...");
 pClient = BLEDevice::createClient();


 if (!pClient->connect(targetDevice)) {
   Serial.println("[BLE] Connection failed");
   return false;
 }
 Serial.println("[BLE] Connected");


 BLERemoteService* pService = pClient->getService(SERVICE_UUID);
 if (!pService) {
   Serial.println("[BLE] Service not found");
   pClient->disconnect();
   return false;
 }


 pCommandChar = pService->getCharacteristic(COMMAND_CHAR_UUID);
 if (!pCommandChar) {
   Serial.println("[BLE] Command characteristic not found");
   pClient->disconnect();
   return false;
 }


 pStatusChar = pService->getCharacteristic(STATUS_CHAR_UUID);
 if (!pStatusChar) {
   Serial.println("[BLE] Status characteristic not found");
   pClient->disconnect();
   return false;
 }


 // Subscribe to status notifications
 if (pStatusChar->canNotify()) {
   pStatusChar->registerForNotify(statusNotifyCallback);
   Serial.println("[BLE] Subscribed to status notifications");
 }


 // Read initial status once on connect
 String initialStatus = pStatusChar->readValue().c_str();
 parseStatus(initialStatus);


 connected = true;
 Serial.println("[BLE] Hub ready");
 return true;
}


// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
 Serial.begin(115200);
 Serial.println("[BOOT] Room hub starting...");
 Serial.println("[INFO] Simulated time starts at 06:00, 1 hour = 30 seconds");
 Serial.println("[INFO] Manual commands: CURTAIN_OPEN / CURTAIN_CLOSE / LED_BLUE / LED_RED / LED_OFF");


 BLEDevice::init("RoomHub");
 BLEScan* pScan = BLEDevice::getScan();
 pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
 pScan->setActiveScan(true);
 Serial.println("[SCAN] Scanning for CurtainNode...");
 pScan->start(10);
}


// ─── Main loop ────────────────────────────────────────────────────────────────
void loop() {


 // Connect if node was found
 if (doConnect && !connected) {
   if (!connectToNode()) {
     Serial.println("[HUB] Connect failed - retrying in 5s");
     delay(5000);
     BLEDevice::getScan()->start(10);
   }
   doConnect = false;
 }


 // Restart scan if disconnected
 if (!connected && !doConnect) {
   static unsigned long lastScan = 0;
   if (millis() - lastScan > 10000) {
     Serial.println("[BLE] Not connected - scanning...");
     BLEDevice::getScan()->start(10);
     lastScan = millis();
   }
 }


 // Time-based logic - only runs when connected
 if (connected) {
   int hour = getCurrentHour();


   static int lastLoggedHour = -1;
   if (hour != lastLoggedHour) {
     Serial.print("[TIME] Simulated hour: ");
     Serial.println(hour);
     lastLoggedHour = hour;
   }


   if (hour == MORNING_HOUR && lastAppliedMode != "MORNING") {
     applyMorningMode();
   } else if (hour == NIGHT_HOUR && lastAppliedMode != "NIGHT") {
     applyNightMode();
   }
 }


 // Manual Serial Monitor commands
 if (Serial.available()) {
   String input = Serial.readStringUntil('\n');
   input.trim();
   if (input.length() > 0) {
     Serial.print("[MANUAL] Sending: ");
     Serial.println(input);
     sendCommand(input);
   }
 }


 delay(100);
}



