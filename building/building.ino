// ESP32 FLOOR CONTROLLER
// Board: ESP32
// Libraries: PubSubClient

#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";

const char* MQTT_HOST = "192.168.1.100";   // your broker IP
const int   MQTT_PORT = 1883;

const char* CLIENT_ID = "floor1-esp32";

const char* TOPIC_ROOM_REQ       = "cmd/hotel/building1/room101/req";
const char* TOPIC_ROOM_RES       = "cmd/hotel/building1/room101/res";
const char* TOPIC_FLOOR_REQ      = "cmd/hotel/building1/floor-controller/req";
const char* TOPIC_FLOOR_RES      = "cmd/hotel/building1/floor-controller/res";
const char* TOPIC_ROOM_TELEMETRY = "dt/hotel/building1/room101/status";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

unsigned long lastRoomPollMs = 0;

String extractValue(const String& json, const String& key) {
  String pattern = "\"" + key + "\":\"";
  int start = json.indexOf(pattern);
  if (start < 0) return "";
  start += pattern.length();
  int end = json.indexOf("\"", start);
  if (end < 0) return "";
  return json.substring(start, end);
}

void requestRoomStatus() {
  String payload = "{";
  payload += "\"request_id\":\"roomreq-" + String(millis()) + "\",";
  payload += "\"action\":\"get_status\"";
  payload += "}";

  client.publish(TOPIC_ROOM_REQ, payload.c_str());
  Serial.println("Requested room status");
}

void replyWithTime(const String& requestId, const String& room) {
  // Replace this with NTP or RTC later
  String fakeTime = "2026-04-14T13:52:00";

  String payload = "{";
  payload += "\"request_id\":\"" + requestId + "\",";
  payload += "\"ok\":true,";
  payload += "\"time\":\"" + fakeTime + "\",";
  payload += "\"floor\":\"floor1\",";
  payload += "\"target_room\":\"" + room + "\"";
  payload += "}";

  client.publish(TOPIC_FLOOR_RES, payload.c_str());
  Serial.println("Sent time response to room");
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

  if (topicStr == TOPIC_ROOM_RES || topicStr == TOPIC_ROOM_TELEMETRY) {
    Serial.println("Room data received by floor controller");
    // Later: forward this to your central ESP32 here
  }
  else if (topicStr == TOPIC_FLOOR_REQ) {
    String action = extractValue(msg, "action");
    String requestId = extractValue(msg, "request_id");
    String fromRoom = extractValue(msg, "from");

    if (action == "get_time") {
      replyWithTime(requestId, fromRoom);
    }
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

      client.subscribe(TOPIC_ROOM_RES);
      client.subscribe(TOPIC_ROOM_TELEMETRY);
      client.subscribe(TOPIC_FLOOR_REQ);

      Serial.println("Subscribed to room/floor topics");
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
  if (now - lastRoomPollMs > 15000) {
    lastRoomPollMs = now;
    requestRoomStatus();
  }
}