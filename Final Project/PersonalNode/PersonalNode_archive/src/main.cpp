#include <Arduino.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "esp_sleep.h"

#define IS_DEBUG false

//Operation modes
enum DeviceMode {
  INITIATE,
  HR_LOWFREQ,
  HR_HIGHFREQ,
  HR_GYRO,
};

//ESP definitions
const uint8_t LIGHT_SLEEP = 30;          // Time in seconds to sleep for in light sleep mode  
const uint16_t HR_LOWFREQ_SLEEP = 600;   // Time in seconds to sleep for in low frequency heart rate acquisition mode
const uint8_t HR_HIGHFREQ_SLEEP = 120;    // Time in seconds to sleep for in high frequency heart rate acquisition mode


//Pin definitions
const uint8_t RXD2_LORA = 16;    // LoRa RX pin, 16 on ESP32-doit, 4 on ESP32-C3
const uint8_t TXD2_LORA = 17;    // LoRa TX pin, 17 on ESP32-doit, 5 on ESP32-C3
const uint8_t RST_LORA = 5;      // LoRa reset pin, 5 on ESP32-doit, 3 on ESP32-C3
//const uint8_t SDA_HR = 8;        // Heart rate sensor SDA pin
//const uint8_t SCL_HR = 9;        // Heart rate sensor SCL pin

//LoRaWAN credentials
const char* APP_EUI   = "BE7A000000001465";   // 8‑byte hex AppEUI/JoinEUI
const char* DEV_EUI   = "0004A30B010D06C5";   // 8‑byte hex DevEUI
const char* APP_KEY   = "858DE1AB60537AA325F4A6D5ED4EC136";  // 16‑byte hex AppKey

//LoRa settings
#define BAUD_RATE 57600    // LoRa baud rate
#define SERIAL_TIME_OUT 1500        // LoRa serial timeout in milliseconds
#define TIME_OUT 1500               // General timeout for LoRa commands in milliseconds
RTC_DATA_ATTR uint32_t LORA_FREQ = 868100000; // LoRa frequency (868.1 MHz for EU)
RTC_DATA_ATTR int8_t LORA_PWR = 14; // LoRa transmission power (dBm)
RTC_DATA_ATTR uint8_t LORA_SF = 7;  // LoRa spreading factor
RTC_DATA_ATTR uint16_t LORA_BW = 125; // LoRa bandwidth (kHz)
const uint8_t MAX_RETRIES = 10; // Maximum number of retries for sending distress signal
const uint16_t RETRY_INTERVAL_MS = 30000; // Time to wait between retries in milliseconds

//Heart rate acquisition settings
const uint32_t ACQUISITION_DURATION = 20000; // Heart rate acqusition window. Time limited by int size

//Module instances
HardwareSerial loraSerial(1); // LoRa serial instance
//MAX30105 particleSensor; // Heart rate sensor instance

RTC_DATA_ATTR DeviceMode currentMode = INITIATE;

String toHex(const String& s);

String fromHex(const String& hex);

void LoRa_flushInput();

String LoRa_cmd(String s);

bool LoRa_initiate();

String LoRa_WANTX(String payload);

bool LoRa_WANdistress();

bool LoRa_switchP2P(uint32_t freq, uint8_t pwr, uint8_t sf, uint8_t bw);

bool LoRa_P2PTX(const String& payload);

String LoRa_P2PRX(uint32_t timeout);

bool LoRa_P2PPing();

DeviceMode LoRa_getMode();

bool HR_initiate();

void HR_readSensor();

bool GYRO_initiate();

void HR_GYRO_readSensor();

void enterSleep(uint32_t duration, bool deep);

void setup(){
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

  //Initialise heart rate sensor
  if(!HR_initiate()) {
    if(IS_DEBUG) {
      Serial.println("Failed to initialise heart rate sensor. Check wiring.");
    }
    while(1);
  }else if(IS_DEBUG) {
    Serial.println("Heart rate sensor initialised successfully.");
  }
}

void loop(){
  switch (currentMode)
  {
  case INITIATE:
    // Initialization code
    if(!LoRa_switchP2P(LORA_FREQ, LORA_PWR, LORA_SF, LORA_BW)) {
      enterSleep(LIGHT_SLEEP, false); // Enter light sleep on failure to save power
    }
    else {
      currentMode = LoRa_getMode();
    }
  break;

  case HR_LOWFREQ:
    // Heart rate acquisition at low frequency
    HR_readSensor();
    currentMode = LoRa_getMode();
    if(IS_DEBUG) {
      Serial.println("Entering low frequency sleep mode for " + String(HR_LOWFREQ_SLEEP) + " seconds.");
    }
    enterSleep(HR_LOWFREQ_SLEEP, false); // Enter light sleep
  break;

  case HR_HIGHFREQ:
    // Heart rate acquisition at high frequency
    HR_readSensor();
    currentMode = LoRa_getMode();
    if(IS_DEBUG) {
      Serial.println("Entering high frequency sleep mode for " + String(HR_HIGHFREQ_SLEEP) + " seconds.");
    }
    enterSleep(HR_HIGHFREQ_SLEEP, true); // Enter deep sleep
  break;

  case HR_GYRO:
    // Combined heart rate and gyroscope acquisition
    HR_GYRO_readSensor();
    enterSleep(HR_HIGHFREQ_SLEEP, true); // Enter deep sleep
  break;
  }
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

String LoRa_cmd(String s) {
  LoRa_flushInput();
  loraSerial.println(s);
  String response = loraSerial.readStringUntil('\n');
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

bool LoRa_WANdistress() {
  String cmd = "mac reset 868";
  String response = LoRa_cmd(cmd); 
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to reset LoRaWAN stack. Response: " + response);
    }
    return false;
  }
  cmd = "mac pause";
  response = LoRa_cmd(cmd);
  if(response.toInt() == 0) {
    if(IS_DEBUG) {
      Serial.println("Failed to pause LoRaWAN stack. Response: " + response);
    }
    return false;
  }  
  cmd = "mac set deveui " + String(DEV_EUI);
  response = LoRa_cmd(cmd);
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to set LoRaWAN credentials. Response: " + response);
    }
    return false;
  }
  cmd = "mac set appeui " + String(APP_EUI);
  response = LoRa_cmd(cmd);
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to set LoRaWAN credentials. Response: " + response);
    }
    return false;
  }
  cmd = "mac set appkey " + String(APP_KEY);
  response = LoRa_cmd(cmd);
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to set LoRaWAN credentials. Response: " + response);
    }
    return false;
  }
  cmd = "mac resume";
  response = LoRa_cmd(cmd);
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to resume LoRaWAN stack. Response: " + response);
    }
    return false;
  }
  response = LoRa_cmd("mac join otaa");
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to join LoRaWAN network. Response: " + response);
    }
    return false;
  }
  // Wait for join acceptance
  response = loraSerial.readStringUntil('\n');
  if(IS_DEBUG) {
    Serial.println("Join response: " + response);
  }
  if(response == "accepted") {
    if(IS_DEBUG) {
      Serial.println("Successfully joined LoRaWAN network.");
    }
    for (uint8_t i = 0; i < MAX_RETRIES; i++) {
      String resp = LoRa_cmd("mac tx cnf 1 44495354524553530A");
      if (resp == "ok") {
        String txResult = loraSerial.readStringUntil('\n');
        txResult.trim();
        if (txResult == "mac_tx_ok") return true;
      }
      delay(RETRY_INTERVAL_MS);
    }
    if(IS_DEBUG) {
      Serial.println("Failed to send distress signal after " + String(MAX_RETRIES) + " attempts.");
    }
  }
  return false;
}

bool LoRa_switchP2P(uint32_t freq, uint8_t pwr, uint8_t sf, uint8_t bw) {
  String response = LoRa_cmd("mac pause");
  /*if(response.toInt() == 0) {
    if(IS_DEBUG) {
      Serial.println("Failed to pause LoRaWAN stack. Response: " + response);
    }
    return false;
  }*/
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

String LoRa_P2PRX(uint32_t timeout = TIME_OUT) {
  loraSerial.setTimeout(timeout);
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
}

bool LoRa_P2PPing() {
  if (!LoRa_P2PTX("PING")) return false;
  LoRa_P2PRX();                          // consume "radio_tx_ok"
  LoRa_cmd("radio rx 0");
  return LoRa_P2PRX() == "PONG";         // clean string comparison
}

DeviceMode LoRa_getMode() {
  LoRa_P2PTX("MODE?");
  LoRa_P2PRX(); // consume "radio_tx_ok"
  LoRa_cmd("radio rx 0");
  String response = LoRa_P2PRX();
  if (response == "1") return HR_LOWFREQ;
  if (response == "2") return HR_HIGHFREQ;
  if (response == "3") return HR_GYRO;
  if(IS_DEBUG) {
    Serial.println("Unknown mode received: " + response + ". Defaulting to INITIATE and enter light sleep.");
    Serial.flush();
    delay(100);
  }
  enterSleep(LIGHT_SLEEP, false); // Enter light sleep on unknown mode to save power
  return currentMode;
}

bool HR_initiate() {
  /*if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    return false;
  }
  particleSensor.setup(); // Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); // Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); // Turn off Green LED
  */
  if(IS_DEBUG) {
    Serial.println("Heart rate sensor successfully initialised.");
  }
  return true;
}

void HR_readSensor() {
  uint8_t BPM, SpO2;
  //placeholder for sensor reading logic. For demonstration, predefined values
  BPM = 75; // Example BPM value
  SpO2 = 98; // Example SpO2 value

  //Send data over LoRa
  String payload = "BPM:" + String(BPM) + ",SpO2:" + String(SpO2);
  LoRa_P2PTX(payload);
}

bool GYRO_initiate() {
  // Gyroscope initialization code would go here
  if(IS_DEBUG) {
    Serial.println("Gyroscope sensor successfully initialised.");
  }
  return true;
}

void HR_GYRO_readSensor() {
  HR_readSensor();

  if(random(10) == 0) { // Simulate a fall event with 10% probability
    LoRa_WANdistress();
    LoRa_switchP2P(LORA_FREQ, LORA_PWR, LORA_SF, LORA_BW); // Switch back to P2P mode after sending distress signal
  }
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