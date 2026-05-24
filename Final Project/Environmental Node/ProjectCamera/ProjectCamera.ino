#include "esp_camera.h"
#include "ESP_I2S.h"
#include "DHT.h"

// Camera pins
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

// DHT11
#define DHT_PIN  4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// Node identity
#define NODE_ID "N1"

// Tuning
#define CALIBRATION_SECONDS  15
#define CAM_MULTIPLIER       1.5
#define MIC_MULTIPLIER       2
#define SEND_COOLDOWN_MS     3000
#define FRAME_INTERVAL_MS    500

I2SClass I2S;

static uint8_t* prevBuf = nullptr;
static size_t   prevLen = 0;

float baselineCam = 10.0;
float baselineMic = 50.0;

unsigned long lastMotionTrigger = 0;
unsigned long lastSoundTrigger  = 0;
unsigned long lastFrameCheck    = 0;

// Event counters
int motionCount = 0;
int soundCount  = 0;

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size   = FRAMESIZE_QQVGA;
  config.fb_count     = 1;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  return esp_camera_init(&config) == ESP_OK;
}

int32_t readMicAmplitude() {
  int32_t minVal = 32767, maxVal = -32768;
  for (int i = 0; i < 256; i++) {
    int sample = I2S.read();
    if (sample && sample != -1 && sample != 1) {
      if (sample > maxVal) maxVal = sample;
      if (sample < minVal) minVal = sample;
    }
  }
  if (maxVal == -32768 || minVal == 32767) return 0;
  return maxVal - minVal;
}

long readCamDiff() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return -1;

  long meanDiff = 0;
  if (prevBuf != nullptr && prevLen == fb->len) {
    long diff = 0;
    for (size_t i = 0; i < fb->len; i++) {
      diff += abs((int)fb->buf[i] - (int)prevBuf[i]);
    }
    meanDiff = diff / (long)fb->len;
  }

  if (prevBuf == nullptr || prevLen != fb->len) {
    if (prevBuf) free(prevBuf);
    prevBuf = (uint8_t*)malloc(fb->len);
    prevLen = fb->len;
  }
  if (prevBuf) memcpy(prevBuf, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return meanDiff;
}

void calibrate() {
  Serial.println("CALIBRATING keep the room still and quiet");

  float camSum = 0, micSum = 0;
  int   camCount = 0, micCount = 0;

  unsigned long start = millis();
  unsigned long calibDuration = CALIBRATION_SECONDS * 1000UL;

  for (int i = 0; i < 5; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(100);
  }

  while (millis() - start < calibDuration) {
    long diff = readCamDiff();
    if (diff >= 0) { camSum += diff; camCount++; }

    int32_t amp = readMicAmplitude();
    micSum += amp;
    micCount++;

    unsigned long remaining = CALIBRATION_SECONDS - ((millis() - start) / 1000);
    Serial.print("Calibrating... ");
    Serial.print(remaining);
    Serial.println("s remaining");

    delay(500);
  }

  if (camCount > 0) baselineCam = camSum / camCount;
  if (micCount > 0) baselineMic = micSum / micCount;

  if (baselineCam < 3.0)  baselineCam = 3.0;
  if (baselineMic < 30.0) baselineMic = 30.0;

  Serial.println("CALIBRATION DONE");
  Serial.print("CAM baseline: ");  Serial.println(baselineCam);
  Serial.print("MIC baseline: ");  Serial.println(baselineMic);
  Serial.print("CAM threshold: "); Serial.println(baselineCam * CAM_MULTIPLIER);
  Serial.print("MIC threshold: "); Serial.println(baselineMic * MIC_MULTIPLIER);
  Serial.println("SLEEP TRACKER RUNNING");
}

void sendReport() {
  float temp     = dht.readTemperature();
  float humidity = dht.readHumidity();

  String report;
  if (!isnan(temp) && !isnan(humidity)) {
    report = String(NODE_ID) + ",REPORT," +
             String(motionCount) + "," +
             String(soundCount)  + "," +
             String(temp, 1)     + "," +
             String(humidity, 1);
  } else {
    report = String(NODE_ID) + ",REPORT," +
             String(motionCount) + "," +
             String(soundCount)  + ",0,0";
  }

  Serial1.println(report);
  Serial.println("Sent report: " + report);

  // Reset counters after sending
  motionCount = 0;
  soundCount  = 0;
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 44, 43);
  delay(2000);

  if (!initCamera()) {
    Serial.println("ERROR Camera init failed");
    while (true) delay(1000);
  }

  I2S.setPinsPdmRx(42, 41);
  if (!I2S.begin(I2S_MODE_PDM_RX, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("ERROR Mic init failed");
    while (true) delay(1000);
  }

  dht.begin();
  calibrate();
}

void loop() {
  unsigned long now = millis();

  // Check for wake signal from room node
  if (Serial1.available()) {
    String cmd = Serial1.readStringUntil('\n');
    cmd.trim();
    if (cmd == "WAKE") {
      Serial.println("Wake signal received sending report");
      sendReport();
    }
  }

  // Camera motion count
  if (now - lastFrameCheck >= FRAME_INTERVAL_MS) {
    lastFrameCheck = now;
    long diff = readCamDiff();
    if (diff > baselineCam * CAM_MULTIPLIER) {
      if (now - lastMotionTrigger >= SEND_COOLDOWN_MS) {
        motionCount++;
        Serial.print("Motion count: ");
        Serial.println(motionCount);
        lastMotionTrigger = now;
      }
    }
  }

  // Mic sound count
  int32_t amp = readMicAmplitude();
  if (amp > baselineMic * MIC_MULTIPLIER) {
    if (now - lastSoundTrigger >= SEND_COOLDOWN_MS) {
      soundCount++;
      Serial.print("Sound count: ");
      Serial.println(soundCount);
      lastSoundTrigger = now;
    }
  }

  delay(300);
}