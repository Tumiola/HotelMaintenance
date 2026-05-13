#include <Arduino.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "esp_sleep.h"
#include <driver/rtc_io.h>
#include "spo2_algorithm.h"

#define IS_DEBUG true

//Operation modes
enum DeviceMode {
  INITIATE,
  HR_LOWFREQ,
  HR_HIGHFREQ,
  HR_GYRO,
};

//ESP definitions
const uint8_t LIGHT_SLEEP = 60;          // Time in seconds to sleep for in light sleep mode  
const uint16_t HR_LOWFREQ_SLEEP = 600;   // Time in seconds to sleep for in low frequency heart rate acquisition mode
const uint8_t HR_HIGHFREQ_SLEEP = 30;    // Time in seconds to sleep for in high frequency heart rate acquisition mode
const uint8_t MAX_RETRIES = 5;           // Maximum number of retries for sending distress signal
const uint16_t RETRY_INTERVAL_MS = 2000; // Time in milliseconds between retries

//Pin definitions
const uint8_t RXD2_LORA = 16;    // LoRa RX pin, 4 on ESP32-doit, 4 on ESP32-C3 -- moved from general UART2 PINs, to try to solve sleeping issue
const uint8_t TXD2_LORA = 17;    // LoRa TX pin, 2 on ESP32-doit, 5 on ESP32-C3
const uint8_t RST_LORA = 5;      // LoRa reset pin, 5 on ESP32-doit, 3 on ESP32-C3
const uint8_t SDA_HR = 21;        // Heart rate sensor SDA pin
const uint8_t SCL_HR = 22;        // Heart rate sensor SCL pin
const uint8_t GPIO_INTERRUPT = 15; // GPIO pin for wakeup interrupt (e.g., distress signal)

//LoRaWAN credentials
const char* APP_EUI   = "BE7A00000000165C";   // 8‑byte hex AppEUI/JoinEUI
const char* DEV_EUI   = "BE7A0000000002A6";   // 8‑byte hex DevEUI
const char* APP_KEY   = "9142F57ED60231FE70C7B49FB3E166B7";  // 16‑byte hex AppKey

//LoRa settings
#define BAUD_RATE 57600    // LoRa baud rate
#define LORA_TIME_OUT 15000        // LoRa serial timeout in milliseconds
#define TIME_OUT 15000               // General timeout for LoRa commands in milliseconds

//Heart rate acquisition settings
const uint32_t ACQUISITION_DURATION = 20000; // Heart rate acqusition window. Time limited by int size

// HR data buffer
long lastBeat = 0; //Time at which the last beat occurred
float beatsPerMinute;
int beatAvg;
long acqusitionStartTime = 0;
long currentTime = 0;

//Module instances
HardwareSerial loraSerial(1); // LoRa serial instance
MAX30105 particleSensor; // Heart rate sensor instance

RTC_DATA_ATTR DeviceMode currentMode = INITIATE;
volatile bool distressTriggered = false;

String toHex(const String& s);

String fromHex(const String& hex);

void LoRa_flushInput();

String LoRa_cmd(String s);

bool LoRa_initiate(bool first);

bool LoRa_WANTransmission(String payload);

void LoRaWANDistress();

bool HR_initiate();

void HR_readSensor();

void enterSleep(uint32_t duration, bool deep);

void sleepRN2483();

void wakeRN2483();

void configureInterruptPin();

void IRAM_ATTR distressISR();

void configureOperationInterupt();

bool takeDistressFlag();

void setup(){
  //Initialise serial communication
  if(IS_DEBUG) {
    Serial.begin(BAUD_RATE);
    Serial.println("Serial monitor started!");
  }
  esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
  if (wakeupReason == ESP_SLEEP_WAKEUP_EXT0) {   // Enters if woken up from deepsleep due to DISTRESS interupt
    if(IS_DEBUG) {
      Serial.println("Woke up from external interrupt. DISTRESS event triggered.");
      Serial.flush();   // ← same fix on the wake side, before any heavy operations
    }
    delay(100);
    LoRa_initiate(false);       // Must reinitate the module after deepsleep. Discussed above
    LoRaWANDistress(); // Send distress signal on wakeup from external interrupt
    configureOperationInterupt();
  } else 
  if(wakeupReason == ESP_SLEEP_WAKEUP_TIMER) {      // Enters if woken up from deepsleep due to normal operation
    configureOperationInterupt();
    if(IS_DEBUG) {
      Serial.println("Woke up from timer. Current mode: " + String(currentMode));
      Serial.flush();   // ← same fix on the wake side, before any heavy operations
    }
    //This is indeed not great, it should not need to be initiated again. However after extensive debugging, the module cannot reconnect consistently after ESP deep-sleep. Further discussed in the report
    LoRa_initiate(false);
    // Sad sad
    delay(100);
    //wakeRN2483(); Would need if it were to correctly exit the deep sleep. However as noted above complete reinitation is necesarry
  }
  else {
    configureOperationInterupt();
    if(IS_DEBUG) {
      while(Serial.available() == 0) {
        // Wait for serial connection, operator must press a button
      }
      Serial.println("Serial communication initialised.");
      Serial.println("Woke up from other reason: " + String(wakeupReason));
    }
    if(!LoRa_initiate(true)) {
      if(IS_DEBUG) {
        Serial.println("Failed to initialise heart rate sensor. Check wiring.");
      }
    }
    
  }
  if(!HR_initiate()) {
    if(IS_DEBUG) {
      Serial.println("Failed to initialise heart rate sensor. Check wiring.");
    }
  } 
  
  if(takeDistressFlag()){
    LoRaWANDistress();
  }
}

void loop(){
  switch (currentMode)
  {
  case INITIATE:
    // Initial state, parking in lightsleep while operator configures device
    LoRa_WANTransmission("INITIATED");
    if(takeDistressFlag()){
      LoRaWANDistress();
    }
  break;

  case HR_LOWFREQ:
    // Heart rate acquisition at low frequency
    HR_readSensor();                    // The Function itself sends the recorded values
    if(takeDistressFlag()){
      LoRaWANDistress();
    }
  break;

  case HR_HIGHFREQ:
    // Heart rate acquisition at high frequency
    HR_readSensor();
    if(takeDistressFlag()){
      LoRaWANDistress();
    }
  break;
  }
  switch(currentMode){
    case INITIATE:
      if(IS_DEBUG) {
        Serial.println("No mode change received after initiation. Entering light sleep to save power until next transmission opportunity.");
        }
      enterSleep(LIGHT_SLEEP, false); // Enter light sleep after initialization to save power until next transmission opportunity
      break;
    
    case HR_LOWFREQ:
      enterSleep(HR_LOWFREQ_SLEEP, true); // Enter deep sleep for low frequency mode
      break;
    
    case HR_HIGHFREQ:
      enterSleep(HR_HIGHFREQ_SLEEP, true); // Enter deep sleep for high frequency mode
        break;
  }
}

String toHex(const String& s) {         // To convert strigns to HEX format for transmission
  String hex = "";
  for (size_t i = 0; i < s.length(); i++) {
    char buf[3];
    sprintf(buf, "%02X", (uint8_t)s[i]);
    hex += buf;
  }
  return hex;
}

String fromHex(const String& hex) {     // To convert HEX transmissions to readable string values. Not relevant in this applicaiton
  String result = "";
  for (size_t i = 0; i + 1 < hex.length(); i += 2) {
    char buf[3] = { hex[i], hex[i+1], '\0' };
    result += (char)strtol(buf, nullptr, 16);
  }
  return result;
}

void LoRa_flushInput() {              // Wait for LoRa serial to finish any processes, avoid collissions
  while (loraSerial.available()) {
    loraSerial.read();
  }
}

String LoRa_cmd(String s) {           // Raw serial command function, to format command and answer appropiatly
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

bool LoRa_initiate(bool first = true) {   // Use this funciton to initate the LoRa module. bool first has been introduced, so initiation after deep sleep does not run MAC configuration again
  // Initialize LoRa serial communication
  loraSerial.begin(BAUD_RATE, SERIAL_8N1, RXD2_LORA, TXD2_LORA);
  loraSerial.setTimeout(LORA_TIME_OUT);

  // Cycle RST pin to get LoRa module to known state.
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
  String cmd = "";
  if(first){
    // Start the LoRaWAN stack and join the network, only enter if its first configuration
    cmd = "mac reset 868";
    response = LoRa_cmd(cmd); 
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
    cmd = "mac set ar on";
    response = LoRa_cmd(cmd);
    if(response != "ok") {
      if(IS_DEBUG) {
        Serial.println("Failed to set LoRaWAN credentials. Response: " + response);
      }
      return false;
    }
    cmd = "mac save";
    response = LoRa_cmd(cmd);
    if(response != "ok") {
      if(IS_DEBUG) {
        Serial.println("Failed to save LoRaWAN configuration. Response: " + response);
      }
      return false;
    }
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
  }

  cmd = "mac set class a";
  response = LoRa_cmd(cmd);
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to set LoRaWAN class. Response: " + response);
    }
    return false;
  }
  return response.length() > 0; // If we get a response, the module is working
}

bool LoRa_WANTransmission(String payload) {
  String hexPayload = toHex(payload);
  String cmd = "mac tx cnf 1 " + hexPayload;
  String downlinkData = "";
  if(IS_DEBUG) {
    Serial.println("Sending payload: " + payload + " (hex: " + hexPayload + ")");
  }
  String response = LoRa_cmd(cmd);
  if(response != "ok") {
    if(IS_DEBUG) {
      Serial.println("Failed to send payload. Response: " + response);
    }
    return false; // Return on failure to avoid changing state
  }
  // Wait for transmission result
  response = loraSerial.readStringUntil('\n');
  if(IS_DEBUG) {
    Serial.println("Transmission response: " + response);
  }
  if (response.startsWith("mac_rx")) {
      // Format: "mac_rx <port> <hexdata>"
      downlinkData = response.substring(response.lastIndexOf(' ') + 1);
      if(IS_DEBUG) {
        Serial.println("Downlink received on port " + response.substring(7, response.indexOf(' ', 7)) + ": " + downlinkData);
      }
  }
  else if (response.startsWith("mac_tx_ok")) {
    if(IS_DEBUG) {
      Serial.println("Payload sent successfully, no downlink received.");
    }
    return true; // Stay in current mode on successful transmission
  } else if (response.startsWith("mac_err")) {
    if(IS_DEBUG) {
      Serial.println("Error sending payload. Response: " + response);
      Serial.println("Entering light sleep to save power until next transmission opportunity.");
    }
    return false; // Return current mode on failure to avoid changing state
  }

  if (downlinkData.startsWith("01")) {
    currentMode = HR_LOWFREQ;
    if(IS_DEBUG) {
      Serial.println("Downlink command received: Switch to HR_LOWFREQ mode.");
    }
    return true;
  }
  if (downlinkData.startsWith("02")){
    currentMode = HR_HIGHFREQ;
    if(IS_DEBUG) {
      Serial.println("Downlink command received: Switch to HR_HIGHFREQ mode.");
    }
    return true;
  }
  if(IS_DEBUG) {
    Serial.println("Unknown mode received: " + response);
    Serial.flush();
    delay(100);
  }
  return true;
}

void LoRaWANDistress() {
  String payload = "DISTRESS";
  for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
    for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
      if(IS_DEBUG) {
        Serial.println("Attempting to send distress signal. Attempt " + String(attempt + 1) + " of " + String(MAX_RETRIES));
      }
      if (LoRa_WANTransmission(payload)) {
        if(IS_DEBUG) {
          Serial.println("Distress signal sent successfully.");
        }
        return; // Distress signal sent successfully, exit function
      }
      // Wait a bit before retrying to avoid spamming the network
      delay(RETRY_INTERVAL_MS);
    }
    LoRa_initiate(true);
  }
}

bool HR_initiate() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    return false;
  }
  particleSensor.setup(); // Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); // Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); // Turn off Green LED
  
  if(IS_DEBUG) {
    Serial.println("Heart rate sensor successfully initialised.");
  }
  particleSensor.shutDown();
  return true;
}

void HR_readSensor() {
  particleSensor.wakeUp();
  delay(100);             //wait for wakeup of sensor
  int i = 0;
  currentTime = millis();
  int BPM_sum = 0;
  particleSensor.setPulseAmplitudeIR(0x1F);
  while(currentTime - acqusitionStartTime < ACQUISITION_DURATION) {
    long irValue = particleSensor.getIR();
    if(checkForBeat(irValue) == true) {
      if(lastBeat == 0) {
        lastBeat = millis(); // Initialize lastBeat on first detection
      } else {
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = 60 / (delta / 1000.0);
        if (beatsPerMinute < 255 && beatsPerMinute > 20) {
          BPM_sum += beatsPerMinute;
          i++;
        }
      }
    }
    currentTime = millis();
  }
  particleSensor.setPulseAmplitudeIR(0x00);
  particleSensor.shutDown();
  // Calculate average BPM
  if (i > 0) {
    beatAvg = BPM_sum / i;
  } else {
    beatAvg = 0; // No beats detected
  }

  if (IS_DEBUG) {
    Serial.print("BeatAvg BPM: ");
    Serial.println(beatAvg);
  }

  String payload = "BPM:" + String(beatAvg);
  payload += ",SpO2:NA";

  LoRa_WANTransmission(payload);
}

void enterSleep(uint32_t duration, bool deep) {
  detachInterrupt(digitalPinToInterrupt(GPIO_INTERRUPT));
  // 1. Arm timer wakeup
  esp_sleep_enable_timer_wakeup(duration * 1000000ULL);

  // 2. Arm GPIO interrupt wakeup
  configureInterruptPin();
  esp_sleep_enable_ext0_wakeup(
    (gpio_num_t)GPIO_INTERRUPT, 1 // Wake on HIGH signal
    );

  if (IS_DEBUG) {
    if (deep) {
      Serial.println("Entering deep sleep for " + String(duration) + " seconds...");
    } else {
      Serial.println("Entering light sleep for " + String(duration) + " seconds...");
    }
    delay(100); // Short delay to ensure message is sent before sleeping
    Serial.flush();   // ← blocks until every byte is physically transmitted
  }

  // ── Deep Sleep ──────────────────────────────────────────────────────────────
  if (deep) {
    LoRa_cmd("mac pause");
    sleepRN2483();
    esp_deep_sleep_start();
  }

  // ── Light Sleep ─────────────────────────────────────────────────────────────
  else {
    sleepRN2483();
    esp_light_sleep_start();

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    if (IS_DEBUG) {
      if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        Serial.println("Woke up from timer.");
      } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Woke up from external interrupt. DISTRESS event triggered.");
      } else {
        Serial.println("Woke up from other reason: " + String(cause));
      }
      Serial.flush();   // same fix on the wake side, before any heavy operations
    }

    // Wake RN2483 regardless of cause — it needs to be ready for the next TX
    wakeRN2483();

    // Flag an event if woken by interrupt so the caller can react
    if (cause == ESP_SLEEP_WAKEUP_EXT0) {
      LoRaWANDistress();
    }
    configureOperationInterupt();
  }
}

void wakeRN2483() {
  if (IS_DEBUG) Serial.println("[LoRa] Waking up RN2483");

  const uint8_t MAX_WAKE_RETRIES = 5;

  for (uint8_t attempt = 1; attempt <= MAX_WAKE_RETRIES; attempt++) {
    if (IS_DEBUG) {
      Serial.println("[LoRa] Wake attempt " + String(attempt) + "/" + String(MAX_WAKE_RETRIES));
    }

    // 1. Fully stop UART first
    loraSerial.end();
    delay(50);

    // 2. Drive TX manually to create BREAK
    pinMode(TXD2_LORA, OUTPUT);
    digitalWrite(TXD2_LORA, LOW);
    delay(20);   // BREAK pulse

    // 3. Re-enable UART
    loraSerial.begin(BAUD_RATE, SERIAL_8N1, RXD2_LORA, TXD2_LORA);
    loraSerial.setTimeout(LORA_TIME_OUT);
    delay(200);

    // 4. Send auto-baud sync
    loraSerial.write(0x55);
    loraSerial.flush();
    delay(200);

    // 5. Clear garbage
    while (loraSerial.available()) {
      loraSerial.read();
    }

    // 6. Confirm wake with sys get ver
    String resp = LoRa_cmd("sys get ver");
    resp.trim();

    bool wakeOk = resp.length() > 0 &&
                  resp.indexOf("RN2483") != -1;

    if (IS_DEBUG) {
      Serial.print("[LoRa] Wake response: '");
      Serial.print(resp);
      Serial.println("'");
      Serial.println(wakeOk ? "[LoRa] Wake successful." : "[LoRa] Wake failed.");
    }

    if (wakeOk) {
      return;
    }

    delay(200);
  }

  if (IS_DEBUG) {
    Serial.println("[LoRa] All wake attempts failed. Reinitate the module");
  }
  LoRa_initiate(true);
  return;
}

void sleepRN2483() {
  loraSerial.println("sys sleep 600000");
  delay(50);  // give it time to acknowledge and enter sleep
  loraSerial.flush(); // Ensure command is sent before ending UART
  loraSerial.end(); // End UART to save power while RN2483 is asleep
  if (IS_DEBUG) Serial.println("[LoRa] RN2483 sent to sleep.");
}

void configureInterruptPin() {
  rtc_gpio_init((gpio_num_t)GPIO_INTERRUPT);                              // 1. init first
  rtc_gpio_set_direction((gpio_num_t)GPIO_INTERRUPT, RTC_GPIO_MODE_INPUT_ONLY);  // 2. direction
  rtc_gpio_pulldown_en((gpio_num_t)GPIO_INTERRUPT);                       // 3. pull AFTER init
  rtc_gpio_pullup_dis((gpio_num_t)GPIO_INTERRUPT);
}

void configureOperationInterupt() {
  rtc_gpio_deinit((gpio_num_t)GPIO_INTERRUPT);
  pinMode(GPIO_INTERRUPT, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(GPIO_INTERRUPT), distressISR, RISING);
}

void IRAM_ATTR distressISR() {
  distressTriggered = true;
}

bool takeDistressFlag() {
  noInterrupts();
  bool f = distressTriggered;
  distressTriggered = false;
  interrupts();
  return f;
}