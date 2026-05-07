// ESP32 ROOM NODE
// Board: ESP32 / NodeMCU
// Libraries: PubSubClient

#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "tumi";
const char* WIFI_PASS = "tumiolason";
const char* MQTT_HOST = "broker.emqx.io";
const int   MQTT_PORT = 1883;

const char* CLIENT_ID = "room101-esp32";

const char* TOPIC_ROOM_REQ   = "cmd/hotel/building1/room101/req";
const char* TOPIC_ROOM_RES   = "cmd/hotel/building1/room101/res";
const char* TOPIC_FLOOR_REQ  = "cmd/hotel/building1/floor-controller/req";
const char* TOPIC_FLOOR_RES  = "cmd/hotel/building1/floor-controller/res";
const char* TOPIC_TELEMETRY  = "dt/hotel/building1/room101/status";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

unsigned long lastTelemetryMs = 0;
unsigned long lastTimeRequestMs = 0;

float fakeTemperature() {
  return 22.0 + (millis() % 1000) / 100.0;   // 22.0 to 31.9
}

float fakeHumidity() {
  return 40.0 + (millis() % 500) / 10.0;     // 40.0 to 89.9
}

String extractValue(const String& json, const String& key) {
  String pattern = "\"" + key + "\":\"";
  int start = json.indexOf(pattern);
  if (start < 0) return "";
  start += pattern.length();
  int end = json.indexOf("\"", start);
  if (end < 0) return "";
  return json.substring(start, end);
}

void publishRoomStatus(const String& requestId = "") {
  float t = fakeTemperature();
  float h = fakeHumidity();

  String payload = "{";
  if (requestId.length() > 0) {
    payload += "\"request_id\":\"" + requestId + "\",";
  }
  payload += "\"room\":\"room101\",";
  payload += "\"temperature\":" + String(t, 1) + ",";
  payload += "\"humidity\":" + String(h, 1) + ",";
  payload += "\"motion\":false,";
  payload += "\"window_open\":false";
  payload += "}";

  client.publish(TOPIC_ROOM_RES, payload.c_str());
  client.publish(TOPIC_TELEMETRY, payload.c_str());
}

void requestTimeFromFloor() {
  String payload = "{";
  payload += "\"request_id\":\"time-" + String(millis()) + "\",";
  payload += "\"action\":\"get_time\",";
  payload += "\"from\":\"room101\"";
  payload += "}";

  client.publish(TOPIC_FLOOR_REQ, payload.c_str());
  Serial.println("Requested time from floor controller");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("MQTT message on ");
  Serial.print(topicStr);
  Serial.print(": ");
  Serial.println(msg);

  if (topicStr == TOPIC_ROOM_REQ) {
    String action = extractValue(msg, "action");
    String requestId = extractValue(msg, "request_id");

    if (action == "get_status") {
      publishRoomStatus(requestId);
    }
  }
  else if (topicStr == TOPIC_FLOOR_RES) {
    Serial.println("Floor response received:");
    Serial.println(msg);
  }
}

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect(CLIENT_ID)) {
      Serial.println("connected");

      client.subscribe(TOPIC_ROOM_REQ);
      client.subscribe(TOPIC_FLOOR_RES);

      Serial.println("Subscribed to room req and floor res topics");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 3s");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  connectWiFi();

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);
  client.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();

  unsigned long now = millis();

  if (now - lastTelemetryMs > 10000) {
    lastTelemetryMs = now;
    publishRoomStatus();
  }

  if (now - lastTimeRequestMs > 30000) {
    lastTimeRequestMs = now;
    requestTimeFromFloor();
  }
}