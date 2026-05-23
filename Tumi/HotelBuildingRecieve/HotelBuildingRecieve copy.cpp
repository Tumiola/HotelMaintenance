/*
 * Author: JP Meijers
 * 30 September 2016
 */

// CHANGED: use HardwareSerial instead of SoftwareSerial
#include <HardwareSerial.h>

// CHANGED: select ESP32 hardware UART1
HardwareSerial loraSerial(1);

String str;

// CHANGED: added reset pin
const int RST = 23;
#define LED_PIN 13

// Tracks last received counter per device (index = device ID, 1–50)
uint16_t lastCounter[51] = {0};
uint32_t lastSeenMs[51]  = {0};


void setup() {
  pinMode(13, OUTPUT);
  led_off();

  // CHANGED: optional RN2483 reset pin
  pinMode(RST, OUTPUT);
  digitalWrite(RST, HIGH);

  // USB serial monitor
  Serial.begin(57600);
  Serial.println("All working");

  // CHANGED: ESP32 hardware UART on chosen pins
  // RX = GPIO18, TX = GPIO19
  loraSerial.begin(9600, SERIAL_8N1, 18, 19);
  loraSerial.setTimeout(60000);

  // hardware reset after UART init
  digitalWrite(RST, LOW);
  delay(200);
  digitalWrite(RST, HIGH);
  delay(200);

  lora_autobaud();

  led_on();
  delay(1000);
  led_off();

  Serial.println("Initing LoRa");

  str = loraSerial.readStringUntil('\n');      // Read startup response
  Serial.println(str);

  loraSerial.println("sys get ver");           // Check module firmware version
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("mac pause");             // Disable LoRaWAN MAC for raw radio mode
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set mod lora");    // Use LoRa modulation
  loraSerial.println("radio set freq 869100000"); // Frequency = 869.1 MHz
  loraSerial.println("radio set pwr 14");      // TX power = 14 dBm
  loraSerial.println("radio set sf sf7");      // Spreading factor = 7
  loraSerial.println("radio set afcbw 41.7");  // AFC bandwidth
  loraSerial.println("radio set rxbw 125");    // RX bandwidth = 125 kHz
  loraSerial.println("radio set prlen 8");     // Preamble length = 8
  loraSerial.println("radio set crc on");      // Enable CRC
  loraSerial.println("radio set iqi off");     // Disable IQ inversion
  loraSerial.println("radio set cr 4/5");      // Coding rate = 4/5
  loraSerial.println("radio set wdt 60000");   // Watchdog timeout = 60 s
  loraSerial.println("radio set sync 12");     // Sync word = 0x12
  loraSerial.println("radio set bw 125");      // LoRa bandwidth = 125 kHz
}

void loop() {
  Serial.println("waiting for a message");
  // Receiver mode
  loraSerial.println("radio rxstop");
  loraSerial.readStringUntil('\n');  // flush response, don't care what it says

  loraSerial.println("radio rx 0");

  str = loraSerial.readStringUntil('\n');
  if (str.indexOf("ok") == 0) {
    str = "";
    while (str == "") {
      str = loraSerial.readStringUntil('\n');
    }
    if (str.indexOf("radio_rx") == 0) {
      Serial.println(str);
      String payload = str.substring(str.indexOf(' ')+1);
      payload.trim();
      decodeAndLog(payload);
      toggle_led();
    } else {
      Serial.println("WDT timeout - Received nothing");
    }
  } else {
    Serial.println("radio not going into receive mode");
    delay(1000);
  }
}

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

void decodeAndLog(String hexPayload){
  hexPayload.trim();

  if (hexPayload.length() < 8) {
    Serial.println("Packet too short: " + hexPayload);
    return;
  }

  uint8_t  deviceId = (uint8_t)strtol(hexPayload.substring(0, 2).c_str(), NULL, 16);
  uint8_t  msgType  = (uint8_t)strtol(hexPayload.substring(2, 4).c_str(), NULL, 16);
  uint16_t counter  = (uint16_t)strtol(hexPayload.substring(4, 8).c_str(), NULL, 16);

  // Guard against out-of-range device IDs
  if (deviceId < 1 || deviceId > 50) {
    Serial.println("Unknown device ID: " + String(deviceId));
    return;
  }

  lastCounter[deviceId] = counter;
  lastSeenMs[deviceId]  = millis();

  Serial.printf("[RX] Device:%02d  MsgType:0x%02X  Counter:%05d  (%.1fs ago: never)\n",
                deviceId, msgType, counter);
}

void decodeHex(String hexPayload) {
  hexPayload.trim();
  Serial.print("Decoded bytes: ");
  for (int i = 0; i < hexPayload.length() - 1; i += 2) {
    String byteStr = hexPayload.substring(i, i + 2);
    uint8_t val = (uint8_t) strtol(byteStr.c_str(), NULL, 16);
    Serial.print("0x"); Serial.print(val, HEX); Serial.print(" ");
  }
  Serial.println();
}

void toggle_led() {
  digitalWrite(13, !digitalRead(13));
}

void led_on() {
  digitalWrite(13, 1);
}

void led_off() {
  digitalWrite(13, 0);
}
