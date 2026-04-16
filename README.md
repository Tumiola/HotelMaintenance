# MQTT Hotel Demo

This project shows a simple hotel-style MQTT setup with one ESP8266 acting as a room node and one ESP32 acting as a floor controller. It uses the Arduino `PubSubClient` pattern for broker connection, publish, subscribe, and callback handling, and it is designed to be easy to scale later to many rooms on one floor controller.

MQTTX is used as the desktop test client. MQTTX can connect to a broker, subscribe to multiple topics, and publish test payloads while the devices are running.

## Architecture

The communication path is:

`ESP8266 room node -> MQTT broker -> ESP32 floor controller`

The room node publishes telemetry and answers room-specific requests. The floor controller requests room data and answers room-to-floor requests such as time. This follows a topic design where command and telemetry traffic are separated and the topic path goes from general to specific.

## Files

- `esp8266_room_node.ino` - Room device sketch for ESP8266.
- `esp32_floor_controller.ino` - Floor device sketch for ESP32.
- `README.md` - This setup guide.

## Broker settings

### ESP8266 and ESP32

Set these in both Arduino sketches:

```cpp
const char* MQTT_HOST = "broker.emqx.io";
const int MQTT_PORT = 1883;
```

The Arduino PubSubClient setup uses broker host and port directly through `setServer()` rather than the WebSocket path used by desktop tools.

### MQTTX desktop client

Create an MQTTX connection with the following values:

- Name: anything you want, for example `FinalProject`
- Host: `broker.emqx.io`
- Protocol: `wss://`
- Port: `8084`
- Path: `/mqtt`
- Username: leave empty
- Password: leave empty

That matches the broker profile used in the desktop screenshot for this project. The ESP boards still use port `1883`, while MQTTX can use secure WebSocket for the same broker.

## Required libraries

Install these libraries in Arduino IDE:

- `PubSubClient`
- `ESP8266WiFi` for the ESP8266 sketch
- `WiFi` for the ESP32 sketch

PubSubClient provides the core MQTT client methods used in this project, including server setup, publish, subscribe, and the callback that handles received messages.

## Topics

Use these topics exactly for the first test:

| Purpose | Topic |
|---|---|
| Room request | `cmd/hotel/floor1/room101/req` |
| Room response | `cmd/hotel/floor1/room101/res` |
| Floor request | `cmd/hotel/floor1/floor-controller/req` |
| Floor response | `cmd/hotel/floor1/floor-controller/res` |
| Room telemetry | `dt/hotel/floor1/room101/status` |

The topic naming keeps telemetry separate from commands and leaves room for later expansion to more rooms and a central controller.

## MQTTX subscriptions

After creating the MQTTX connection, subscribe to these topics:

- `cmd/hotel/floor1/room101/req`
- `cmd/hotel/floor1/room101/res`
- `cmd/hotel/floor1/floor-controller/req`
- `cmd/hotel/floor1/floor-controller/res`
- `dt/hotel/floor1/room101/status`

MQTTX supports topic subscription and publish testing from the desktop client, which makes it useful for watching both device directions at the same time.

## Test sequence

1. Flash `esp8266_room_node.ino` to the ESP8266.
2. Flash `esp32_floor_controller.ino` to the ESP32.
3. Open Serial Monitor for each board at `115200` baud.
4. Connect MQTTX to the broker.
5. Add the five subscriptions listed above.
6. Power both boards and watch the traffic.

When the demo is working:

- The ESP32 publishes a room status request every 15 seconds.
- The ESP8266 replies with temperature and humidity data.
- The ESP8266 also publishes telemetry every 10 seconds.
- The ESP8266 requests time from the ESP32 every 30 seconds.
- The ESP32 replies on the floor response topic.

## Manual publish tests in MQTTX

You can test the system manually from MQTTX even before both boards are running.

### Ask the room for status

Publish to topic:

```text
cmd/hotel/floor1/room101/req
```

Payload:

```json
{"request_id":"manual-1","action":"get_status"}
```

Expected result: the room sketch publishes a JSON response on `cmd/hotel/floor1/room101/res` and also publishes telemetry on `dt/hotel/floor1/room101/status`.

### Ask the floor controller for time

Publish to topic:

```text
cmd/hotel/floor1/floor-controller/req
```

Payload:

```json
{"request_id":"manual-2","action":"get_time","from":"room101"}
```

Expected result: the floor controller publishes a JSON response on `cmd/hotel/floor1/floor-controller/res`.

## How to clone this for another room

To create a second room such as `room102`:

1. Copy the ESP8266 room sketch.
2. Change `CLIENT_ID` to something unique such as `hotel-floor1-room102`.
3. Change the three room topics from `room101` to `room102`.
4. Keep the floor controller topics the same.
5. Either update the floor controller to poll `room102` as well or later switch it to wildcard subscriptions.

A good next step for scaling is to use more general subscriptions on the floor controller, for example with single-level topic wildcards, instead of hardcoding one room topic at a time.

## Notes

- The demo uses placeholder temperature and humidity values so the MQTT flow can be verified first.
- Replace the placeholder sensor functions with a real DHT or similar sensor when the message flow is stable.
- Replace the fixed time string on the ESP32 with NTP or RTC time later.
- Public brokers are convenient for testing, but production deployments should use a broker you control.
