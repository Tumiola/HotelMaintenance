#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char* ssid = "Askur";
const char* password = "Redmi.Redmi";

const char* url = "https://helloworld-wzzedmg5ga-uc.a.run.app";
const char* deviceId = "esp32-001";
const char* apiSecret = "DEVICE_SECRET"; // better stored securely

String makeBody() {
  return "{\"device_id\":\"" + String(deviceId) + "\",\"temperature\":23.7,\"timestamp\":1713870000}";
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi");

  WiFiClientSecure client;
  client.setInsecure(); // replace with proper cert validation in production

  HTTPClient https;
  String body = makeBody();

  // if (https.begin(client, url)) {
  //   https.addHeader("Content-Type", "application/json");

  //   // Example custom auth header; backend verifies it
  //   https.addHeader("X-Device-Id", deviceId);
  //   https.addHeader("X-Signature", "computed_hmac_here");

  //   int code = https.POST(body);
  //   String resp = https.getString();

  //   Serial.println(code);
  //   Serial.println(resp);

  //   https.end();
  // }
  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      String payload = https.getString();
      Serial.println("Response:");
      Serial.println(payload);
    } else {
      Serial.println("Request failed");
    }
    https.end();
  } else {
    Serial.println("Unable to connect");
  }
}

void loop() {}