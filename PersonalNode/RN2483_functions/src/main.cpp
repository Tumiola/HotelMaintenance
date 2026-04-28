#include <Arduino.h>
#include <HardwareSerial.h>
#include "esp_sleep.h"

#define IS_DEBUG false

//ESP definitions
const uint8_t LIGHT_SLEEP = 30;          // Time in seconds to sleep for in light sleep mode  
const uint16_t HR_LOWFREQ_SLEEP = 600;   // Time in seconds to sleep for in low frequency heart rate acquisition mode
const uint8_t HR_HIGHFREQ_SLEEP = 120;    // Time in seconds to sleep for in high frequency heart rate acquisition mode

//Pin definitions
const uint8_t RXD2_LORA = 16;    // LoRa RX pin, 16 on ESP32-doit, 4 on ESP32-C3
const uint8_t TXD2_LORA = 17;    // LoRa TX pin, 17 on ESP32-doit, 5 on ESP32-C3
const uint8_t RST_LORA = 5;      // LoRa reset pin, 5 on ESP32-doit, 3 on ESP32-C3

//LoRa settings
#define BAUD_RATE 57600    // LoRa baud rate
#define SERIAL_TIME_OUT 1500        // LoRa serial timeout in milliseconds
#define TIME_OUT 1500               // General timeout for LoRa commands in milliseconds
const uint8_t MAX_RETRIES = 10; // Maximum number of retries for sending distress signal
const uint16_t RETRY_INTERVAL_MS = 30000; // Time to wait between retries in milliseconds
//RTC_DATA_ATTR allows these variables to retain their values across deep sleep cycles, enabling persistent configuration without needing to reinitialize on wakeup.
RTC_DATA_ATTR uint32_t LORA_FREQ = 868100000; // LoRa frequency (868.1 MHz for EU)
RTC_DATA_ATTR int8_t LORA_PWR = 14; // LoRa transmission power (dBm)
RTC_DATA_ATTR uint8_t LORA_SF = 7;  // LoRa spreading factor
RTC_DATA_ATTR uint16_t LORA_BW = 125; // LoRa bandwidth (kHz)

//Module instances
HardwareSerial loraSerial(1); // LoRa serial instance


//function declarations
String toHex(const String& s);

String fromHex(const String& hex);

void LoRa_flushInput();

String LoRa_cmd(String s);

bool LoRa_initiate();

bool LoRa_switchP2P(uint32_t freq, uint8_t pwr, uint8_t sf, uint8_t bw);

bool LoRa_P2PTX(const String& payload);

String LoRa_P2PRX(uint32_t timeout);

bool LoRa_P2PPing();

void enterSleep(uint32_t duration, bool deep);


void setup() {
  //Initialise serial communication
  if(IS_DEBUG) {
    Serial.begin(BAUD_RATE);
    while(Serial.available() == 0) {
      // Wait for serial connection
    }
    Serial.println("Serial communication initialised.");
  }

  //Random seed for demonstration
  randomSeed(esp_random());

  //Initialise LoRa module
  if(!LoRa_initiate()) {
    if(IS_DEBUG) {
      Serial.println("Failed to initialise LoRa module. Check wiring and credentials.");
    }
    while(1);
  }else if(IS_DEBUG) {
    Serial.println("LoRa module initialised successfully.");
  }
}

void loop(){
  // Main loop can be used for testing or additional functionality
}

String toHex(const String& s) {
  String hex = "";
  for (size_t i = 0; i < s.length(); i++) {
    char buf[3];
    sprintf(buf, "%02X", (uint8_t)s[i]);
    hex += buf;
  }
  return hex;
}

String fromHex(const String& hex) {
  String result = "";
  for (size_t i = 0; i + 1 < hex.length(); i += 2) {
    char buf[3] = { hex[i], hex[i+1], '\0' };
    result += (char)strtol(buf, nullptr, 16);
  }
  return result;
}

void LoRa_flushInput() {
  while (loraSerial.available()) {
    loraSerial.read();
  }
}

String LoRa_cmd(String s) {                             // Whatch out, it uses Strings, can get messy with heap allocation
  LoRa_flushInput();
  loraSerial.println(s);
  String response = loraSerial.readStringUntil('\n');   // eats any "ok" response after receive
  response.trim();
  if(IS_DEBUG) {
    Serial.print("> ");
    Serial.println(s);
    Serial.print("< ");
    Serial.println(response);
  }
  delay(100); // Short delay to ensure response is fully received before next command
  return response;
}

bool LoRa_initiate() {
  // Initialize LoRa serial communication
  loraSerial.begin(BAUD_RATE, SERIAL_8N1, RXD2_LORA, TXD2_LORA);
  loraSerial.setTimeout(SERIAL_TIME_OUT);

  pinMode(RST_LORA, OUTPUT);
  digitalWrite(RST_LORA, LOW);
  delay(100);
  digitalWrite(RST_LORA, HIGH);
  delay(100);

  // Check if LoRa module is responding
  String response = LoRa_cmd("sys get ver");
  if(IS_DEBUG) {
    Serial.println("LoRa module version: " + response);
  }
  return response.length() > 0; // If we get a response, the module is working
}

bool LoRa_switchP2P(uint32_t freq, uint8_t pwr, uint8_t sf, uint8_t bw) {
  String response = LoRa_cmd("mac pause");                                        // Pause LoRaWAN stack to switch to P2P mode, response is "ok" if successful
  response = LoRa_cmd("radio set freq " + String(freq));
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to set LoRa frequency. Response: " + response);
    }
    return false;
  }
  response = LoRa_cmd("radio set pwr " + String(pwr));
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to set LoRa power. Response: " + response);
    }
    return false;
  }
  response = LoRa_cmd("radio set sf sf" + String(sf));
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to set LoRa spreading factor. Response: " + response);
    }
    return false;
  }
  response = LoRa_cmd("radio set bw " + String(bw));
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to set LoRa bandwidth. Response: " + response);
    }
    return false;
  }

  if(IS_DEBUG) {
    Serial.println("LoRa switched to P2P mode successfully.");
    delay(1000); // Short delay to ensure settings are applied before next command
  }
  return true;
}

bool LoRa_P2PTX(const String& payload) {
  String hexPayload = toHex(payload);

  if (IS_DEBUG) {
    Serial.println("[TX] " + payload + " → " + hexPayload);
  }

  String response = LoRa_cmd("radio tx " + hexPayload);
  if (response != "ok") {
    if (IS_DEBUG) {
      Serial.println("[TX] Failed: " + response);
    }
    return false;
  }
  return true;
}

String LoRa_P2PRX(uint32_t timeout = TIME_OUT) {                // This function is not working properly, maybe you should consider rewriting it. Note switching to rx 0 is not implemented here, its usually done before calling the funciton. Maybe this is also an issue
  /*loraSerial.setTimeout(timeout);
  char buf[256];
  size_t len = loraSerial.readBytesUntil('\n', buf, sizeof(buf) - 1);
  if (len == 0) {
    if (IS_DEBUG) {
      Serial.println("[RX] Timeout waiting for response.");
    }
    return "";
  }
  buf[len] = '\0'; // Null-terminate the string
  String raw = String(buf);
  raw.trim();
  if (!raw.startsWith("radio rx")) {
    if (IS_DEBUG) {
      Serial.print("[RX] Unexpected response: ");
      Serial.println(raw);
    }
    return "";
  }

  // Extract hex payload from "radio_rx  <hex>"
  String hex = raw.substring(raw.lastIndexOf(' ') + 1);
  hex.trim();
  String decoded = fromHex(hex);

  if (IS_DEBUG) {
    Serial.println("[RX] " + hex + " → " + decoded);
  }
  return decoded;
  */
}

bool LoRa_P2PPing() {
  if (!LoRa_P2PTX("PING")) return false;
  LoRa_cmd("radio rx 0");
  return LoRa_P2PRX() == "PONG";         // clean string comparison
}

void enterSleep(uint32_t duration, bool deep) {
  if(deep) {
    if(IS_DEBUG) {
      Serial.println("Entering deep sleep for " + String(duration) + " seconds.");
      Serial.flush();
      delay(100);
    }
    esp_sleep_enable_timer_wakeup(duration * 1000000ULL);
    esp_deep_sleep_start();
  } else {
    if(IS_DEBUG) {
      Serial.println("Entering light sleep for " + String(duration) + " seconds.");
      Serial.flush();
      delay(100);
    }
    esp_sleep_enable_timer_wakeup(duration * 1000000ULL);
    esp_light_sleep_start();
  }
}