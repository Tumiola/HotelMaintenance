#include <Arduino.h>
#include <HardwareSerial.h>

#define IS_DEBUG true

//Operation modes
enum DeviceMode {
  HR_LOWFREQ,
  HR_HIGHFREQ,
  HR_GYRO,
};

DeviceMode currentMode = HR_LOWFREQ;

//Pin definitions
const uint8_t RXD2_LORA = 4;    // LoRa RX pin, 16 on ESP32-doit, 4 on ESP32-C3
const uint8_t TXD2_LORA = 5;    // LoRa TX pin, 17 on ESP32-doit, 5 on ESP32-C3
const uint8_t RST_LORA = 3;      // LoRa reset pin, 5 on ESP32-doit, 3 on ESP32-C3


//LoRa settings
#define BAUD_RATE 57600    // LoRa baud rate
#define SERIAL_TIME_OUT 1500        // LoRa serial timeout in milliseconds
#define TIME_OUT 1500               // General timeout for LoRa commands in milliseconds
uint32_t LORA_FREQ = 868300000; // LoRa frequency (868.1 MHz for EU)
uint8_t LORA_PWR = 14; // LoRa transmission power (dBm)
uint8_t LORA_SF = 7;  // LoRa spreading factor
uint16_t LORA_BW = 125; // LoRa bandwidth (kHz)

//Placeholder for last acquired heart rate and SpO2 values
uint8_t lastBPM  = 0;
uint8_t lastSpO2 = 0;

//Module instances
HardwareSerial loraSerial(1); // LoRa serial instance

String toHex(const String& s);

String fromHex(const String& hex);

void LoRa_flushInput();

String LoRa_cmd(String s);

bool LoRa_initiate();

bool LoRa_P2P(uint32_t freq, uint8_t pwr, uint8_t sf, uint16_t bw);

bool LoRa_P2PTX(const String& payload);

String LoRa_P2PRX(uint32_t timeout);

bool parseBPMPayload(const String& payload, uint8_t& bpm, uint8_t& spo2);

void setup(){
  //Initialise serial communication
  if(IS_DEBUG) {
    Serial.begin(BAUD_RATE);
    while(Serial.available() == 0) {
      // Wait for serial connection
    }
    Serial.println("Serial communication initialised.");
  }

  //Initialise LoRa module
  if(!LoRa_initiate()) {
    if(IS_DEBUG) {
      Serial.println("Failed to initialise LoRa module. Check wiring and credentials.");
    }
    while(1);
  }else if(IS_DEBUG) {
    Serial.println("LoRa module initialised successfully.");
  }

  if(LoRa_P2P(LORA_FREQ, LORA_PWR, LORA_SF, LORA_BW)) {
    if(IS_DEBUG) {
      Serial.println("LoRa switched to P2P mode successfully.");
    }
  } else if(IS_DEBUG) {
    Serial.println("Failed to switch LoRa to P2P mode.");
    while(1){
      Serial.println("Failed to switch LoRa to P2P mode. Check settings and credentials.");
      delay(5000);
    }
  }
}

void loop(){
  LoRa_cmd("radio rx 0"); // Open RX for incoming packets
  
  /*String response = LoRa_P2PRX(5000); // Wait for packet with 5s timeout
  if(response.length() > 0) {
    if(IS_DEBUG) {
      Serial.println("Received: " + response);
    }
    if(response == "PING") {  
      LoRa_P2PTX("PONG");
      LoRa_P2PRX(TIME_OUT); // Consume "radio_tx_ok"  
    }
    else if(response == "MODE?") {
      String modeResponse;
      if(currentMode == HR_LOWFREQ) {
        modeResponse = "1";
      } else if(currentMode == HR_HIGHFREQ) {
        modeResponse = "2";
      } else {
        modeResponse = "3";
      }
      LoRa_P2PTX(modeResponse);
    }
  } else if(IS_DEBUG) {
    Serial.println("No packet received within timeout.");
  }
  if(Serial.available()>0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("MODE ")) {
      String modeStr = cmd.substring(5);
      int mode = modeStr.toInt();
      if (mode == 1) {
        currentMode = HR_LOWFREQ;
      } else if (mode == 2) {
        currentMode = HR_HIGHFREQ;
      } else if (mode == 3) {
        currentMode = HR_GYRO;
      }
      if (response == "PING") {
        LoRa_P2PTX("PONG");
        loraSerial.readStringUntil('\n');  // consume radio_tx_ok
      }
      else if (response == "MODE?") {
        const char* modeMap[] = {"1", "2", "3"};
        LoRa_P2PTX(modeMap[currentMode]);
        loraSerial.readStringUntil('\n');  // consume radio_tx_ok
      }
      else if (response.startsWith("BPM")) {
        if (parseBPMPayload(response, lastBPM, lastSpO2)) {
          if (IS_DEBUG) {
            Serial.println("BPM: "  + String(lastBPM));
            Serial.println("SpO2: " + String(lastSpO2) + "%");
          }
        } else {
          if (IS_DEBUG) Serial.println("[BPM] Parse failed: " + response);
        }
      } else {
        if (IS_DEBUG) {
          Serial.println("Invalid mode: " + modeStr);
        }
        return;
      }
      if (IS_DEBUG) {
        const char* modeNames[] = {"HR_LOWFREQ", "HR_HIGHFREQ", "HR_GYRO"};
        Serial.println("Mode switched to " + String(modeNames[currentMode]));
      }
    }
  }
  LoRa_cmd("radio rxstop"); // Re-open RX for next packet
  */
 String response = loraSerial.readStringUntil('\n');
  response.trim();
  if(response.length()) {
    Serial.print("RN2483: ");
    Serial.println(response);
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
  delay(100); // Small delay to ensure command is processed before next one
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

bool LoRa_P2P(uint32_t freq, uint8_t pwr, uint8_t sf, uint16_t bw) {
  String response;

  // Pause LoRaWAN MAC stack
  LoRa_cmd("mac pause");

  // Set modulation mode to LoRa (not FSK)
  response = LoRa_cmd("radio set mod lora");
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set LoRa mode: " + response);
    return false;
  }

  // Frequency (Hz) — e.g. 868100000
  response = LoRa_cmd("radio set freq " + String(freq));
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set frequency: " + response);
    return false;
  }

  // TX power (dBm) — RN2483 range: -3 to 14
  response = LoRa_cmd("radio set pwr " + String(pwr));
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set power: " + response);
    return false;
  }

  // Spreading factor — sf7 to sf12
  response = LoRa_cmd("radio set sf sf" + String(sf));
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set SF: " + response);
    return false;
  }

  // Bandwidth (kHz) — 125, 250, or 500
  response = LoRa_cmd("radio set bw " + String(bw));
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set BW: " + response);
    return false;
  }

  // Coding rate — 4/5, 4/6, 4/7, 4/8
  response = LoRa_cmd("radio set cr 4/5");
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set CR: " + response);
    return false;
  }

  // Preamble length — default 8, valid 0–65535
  response = LoRa_cmd("radio set prlen 8");
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set preamble: " + response);
    return false;
  }

  // CRC on/off — always on for data integrity
  response = LoRa_cmd("radio set crc on");
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set CRC: " + response);
    return false;
  }

  // IQ inversion — off for P2P (on is used for LoRaWAN downlinks)
  response = LoRa_cmd("radio set iqi off");
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set IQI: " + response);
    return false;
  }

  // Sync word — 0x12 for private network (0x34 for LoRaWAN public)
  response = LoRa_cmd("radio set sync 12");
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set sync word: " + response);
    return false;
  }

  // Watchdog timer — 0 disables it (important! otherwise RX auto-closes)
  response = LoRa_cmd("radio set wdt 0");
  if (response != "ok") {
    if (IS_DEBUG) Serial.println("Failed to set WDT: " + response);
    return false;
  }

  if (IS_DEBUG) Serial.println("LoRa switched to P2P mode successfully.");
  delay(200); // Let radio settle after configuration
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

String LoRa_P2PRX(uint32_t timeout) {
  loraSerial.setTimeout(timeout);
  String raw = loraSerial.readStringUntil('\n');
  raw.trim();

  if (IS_DEBUG) {
    Serial.print("[RX raw] ");
    Serial.println(raw);
  }

  if (!raw.startsWith("radio_rx")) {
    if (IS_DEBUG) Serial.println("[RX] No packet.");
    LoRa_cmd("radio rxstop");
    return "";
  }

  String hex = raw.substring(raw.lastIndexOf(' ') + 1);
  String decoded = fromHex(hex);

  if (IS_DEBUG) Serial.println("[RX] " + hex + " → " + decoded);
  return decoded;
}

bool parseBPMPayload(const String& payload, uint8_t& bpm, uint8_t& spo2) {
  // Expected format: "BPM:075,SPO2:098"
  int bpmColon  = payload.indexOf(':');
  int comma     = payload.indexOf(',');
  int spo2Colon = payload.indexOf(':', comma);

  if (bpmColon == -1 || comma == -1 || spo2Colon == -1) return false;

  bpm  = (uint8_t)payload.substring(bpmColon + 1, comma).toInt();
  spo2 = (uint8_t)payload.substring(spo2Colon + 1).toInt();
  return true;
}