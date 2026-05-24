/******************************************************************************
 * File:        BuildingNode.ino
 * Project:     LORCA - LoRa Medical Hotel Monitoring System
 *
 * Description:
 * Building gateway node responsible for:
 *   - Receiving LoRa uplink payloads from room nodes
 *   - Forwarding payloads to the cloud backend over HTTPS
 *   - Polling backend for pending downlink actuator commands
 *   - Transmitting downlink commands to room nodes over LoRa
 *
 * Author:      Askur Hugi - s251904
 * Group:       Group 6
 * Course:      34346 - Networking technologies and application
 *                      development for Internet of Things (IoT)
 * Institution: Technical University of Denmark (DTU)
 *
 * Date:        2026-05-24
 *
 * Hardware:
 *   - ESP32
 *   - Microchip RN2483 / RN2903 LoRa Module
 *
 * Notes:
 *   - LoRa payloads are forwarded without local decoding
 *   - Payload decoding and validation are handled cloud-side
 *   - Designed as an academic proof-of-concept system
 *
 ******************************************************************************/

#include <HTTPClient.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

const char *ssid = "SSID";
const char *password = "PASSW";

const char *uplinkUrl = "https://helloworld-wzzedmg5ga-uc.a.run.app/uplink";
const char *downlinkPollUrl =
    "https://helloworld-wzzedmg5ga-uc.a.run.app/downlink/poll";

const char *gatewayId = "building-demo-01";
const char *apiSecret = "DEVICE_SECRET";

// RN2xx3 UART setup.
HardwareSerial loraSerial(1);

const int LORA_RX = 18;
const int LORA_TX = 19;
const int RST = 23;
const int LED = 13;

unsigned long lastPollMs = 0;
const unsigned long pollIntervalMs = 60000; // poll every minute

// Checks if a string is valid hex.
bool isHexString(const String &s) {
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
              (c >= 'a' && c <= 'f');

    if (!ok)
      return false;
  }
  return true;
}

// Sends a command to the lora module and prints the response.
String sendCommand(String cmd) {
  loraSerial.println(cmd);
  String response = loraSerial.readStringUntil('\n');
  response.trim();

  Serial.print("> ");
  Serial.println(cmd);
  Serial.print("< ");
  Serial.println(response);

  return response;
}

// Forces the RN module to autobaud after reset.
void lora_autobaud() {
  String response = "";

  while (response == "") {
    delay(1000);
    loraSerial.write((byte)0x00);
    loraSerial.write(0x55);
    loraSerial.println();
    loraSerial.println("sys get ver");

    response = loraSerial.readStringUntil('\n');
    response.trim();
  }
}

// Sends received lora payloads to the backend.
void forwardUplinkToBackend(String payloadHex) {
  WiFiClientSecure client;
  client.setInsecure(); // sketchy but fine for proof of concept

  HTTPClient https;

  // Checks if https connection started.
  if (!https.begin(client, uplinkUrl)) {
    Serial.println("Backend uplink connection failed");
    return;
  }

  // Build JSON payload.
  String body = "{";
  body += "\"gatewayId\":\"" + String(gatewayId) + "\",";
  body += "\"payloadHex\":\"" + payloadHex + "\"";
  body += "}";

  https.addHeader("Content-Type", "application/json");
  https.addHeader("X-Gateway-Id", gatewayId);
  https.addHeader("X-Api-Secret", apiSecret);

  // POST payload.
  int code = https.POST(body);
  String response = https.getString();

  Serial.print("Uplink HTTP code: ");
  Serial.println(code);
  Serial.println(response);

  https.end();
}

// ------------------------------------------------- LoRa DOWNLINK
// -------------------------------------------------

// Sends a backend command down to a room node over lora.
bool passOnLoRaDownlink(String commandHex) {
  commandHex.trim();
  commandHex.toUpperCase();

  // Makes sure the command is valid before transmitting.
  if (commandHex.length() != 6 || !isHexString(commandHex)) {
    Serial.println("Invalid downlink command");
    return false;
  }

  Serial.print("Sending LoRa downlink: ");
  Serial.println(commandHex);

  loraSerial.println("radio tx " + commandHex);

  String response1 = loraSerial.readStringUntil('\n');
  response1.trim();

  String response2 = loraSerial.readStringUntil('\n');
  response2.trim();

  Serial.print("TX response 1: ");
  Serial.println(response1);
  Serial.print("TX response 2: ");
  Serial.println(response2);

  return response1 == "ok" && response2 == "radio_tx_ok";
}

// Polls the backend for pending actuator commands.
void pollDownlink() {
  WiFiClientSecure client;
  // JUST FOR PROOF OF CONCEPT.
  client.setInsecure();

  HTTPClient https;

  String url = String(downlinkPollUrl) + "?gatewayId=" + gatewayId;

  // Checks if HTTPS poll connection could be started.
  if (!https.begin(client, url)) {
    Serial.println("Backend downlink poll connection failed");
    return;
  }

  https.addHeader("X-Gateway-Id", gatewayId);
  https.addHeader("X-Api-Secret", apiSecret);

  int code = https.GET();
  String response = https.getString();
  response.trim();

  Serial.print("Poll HTTP code: ");
  Serial.println(code);
  Serial.print("Poll response: ");
  Serial.println(response);

  https.end();

  // Sends the command over lora if the backend returned one.
  if (code == 200 && response != "NONE") {
    if (response.length() == 6 && isHexString(response)) {
      bool ok = sendLoRaDownlink(response);

      if (ok) {
        Serial.println("Downlink sent");
      } else {
        Serial.println("Downlink failed");
      }
    } else {
      Serial.println("Invalid backend downlink format");
    }
  }
}

// ------------------------------------------------- LoRa module setup
// -------------------------------------------------

// Initializes the RN LoRa module.
void initLoRaModule() {
  pinMode(RST, OUTPUT);
  digitalWrite(RST, HIGH);

  loraSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  loraSerial.setTimeout(60000);

  // hard resets the module just to be safe.
  digitalWrite(RST, LOW);
  delay(200);
  digitalWrite(RST, HIGH);
  delay(200);

  lora_autobaud();

  Serial.println("Initializing RN2xx3 LoRa module");

  sendCommand("sys get ver");
  sendCommand("mac pause");

  // Sets up raw LoRa radio settings.
  sendCommand("radio set mod lora");
  sendCommand("radio set freq 869100000");
  sendCommand("radio set pwr 14");
  sendCommand("radio set sf sf7");
  sendCommand("radio set afcbw 41.7");
  sendCommand("radio set rxbw 125");
  sendCommand("radio set prlen 8");
  sendCommand("radio set crc on");
  sendCommand("radio set iqi off");
  sendCommand("radio set cr 4/5");
  sendCommand("radio set wdt 0");
  sendCommand("radio set sync 12");
  sendCommand("radio set bw 125");
}

// Puts the LoRa module into receive mode.
void startLoRaReceive() {
  loraSerial.println("radio rx 0");

  String response = loraSerial.readStringUntil('\n');
  response.trim();

  Serial.print("RX start response: ");
  Serial.println(response);
}

// Runs once on boot.
void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  Serial.begin(57600);

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  // Waits until WiFi connects.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi connected. ESP32 IP: ");
  Serial.println(WiFi.localIP());

  initLoRaModule();
  startLoRaReceive();

  Serial.println("Building gateway ready");
}

// ------------------------------------------------- Main loop
// ------------------------------------------------- Main runtime loop.
void loop() {
  // Polls backend for actuator commands every minute.
  if (millis() - lastPollMs >= pollIntervalMs) {
    lastPollMs = millis();
    pollDownlink();

    // Goes back into RX after transmitting.
    startLoRaReceive();
  }

  // Handles incoming LoRa packets.
  if (loraSerial.available()) {
    String response = loraSerial.readStringUntil('\n');
    response.trim();

    // Ignores empty lines from the module.
    if (response.length() == 0) {
      return;
    }

    Serial.print("LoRa module: ");
    Serial.println(response);

    // Handles received LoRa payloads.
    if (response.startsWith("radio_rx ")) {
      digitalWrite(LED, HIGH);

      String payloadHex = response.substring(9);
      payloadHex.trim();

      Serial.print("Received LoRa payload: ");
      Serial.println(payloadHex);

      forwardUplinkToBackend(payloadHex);

      digitalWrite(LED, LOW);

      // Returns back to RX after handling the packet.
      startLoRaReceive();
    } else if (response == "radio_err") {
      Serial.println("RX error. Restarting RX.");
      startLoRaReceive();
    }
  }
}