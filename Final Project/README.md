# Hotel Room IoT System

An IoT system for monitoring and automating a hotel room environment. Built with a handful of ESP32s talking over BLE and LoRa.

---

## Nodes

| Node | What it does |
|------|--------------|
| **RoomHub** | Central hub. Connects to BLE nodes, pushes data to Blynk, transmits over LoRa P2P |
| **EnvironmentalNode** | Reads temperature, humidity, motion, and sound — reports to hub via BLE |
| **CircadianNode** | Controls a servo curtain and red/blue LEDs based on time-of-day or manual override |
| **PersonalNode** | Wearable-style node. Tracks heart rate with MAX30105, sends over LoRaWAN, sleeps between readings |
| **BuildingNode** | LoRa receiver on the building side — collects payloads from room nodes |

---

## How it works

The hub scans for BLE nodes on boot, connects, and starts receiving sensor data. Based on time of day it automatically opens/closes the curtain and switches between blue (morning) and red (night) lighting. Everything can also be controlled manually through the Blynk app.

Sensor data is packed into a compact hex payload and sent over LoRa P2P roughly every 10–18 seconds, with random jitter to avoid collisions.

---

## Hardware

- ESP32 (one per node)
- RN2483 LoRa module
- MAX30105 pulse oximeter / heart rate sensor
- Servo motor
- Red + blue LEDs

---

## Dependencies

- [Blynk](https://blynk.io)
- ESP32 BLE Arduino
- ESP32Servo
- SparkFun MAX30105 library

---

## Setup

1. Flash each folder's code onto its own ESP32
2. In `RoomHubProject`, set your own WiFi credentials and Blynk auth token
3. Match LoRa frequency and sync word across hub and building node if you change defaults (`869.1 MHz`, sync `0x12`)
4. Pair BLE nodes by name — hub scans for `"RoomNode1"` and `"CurtainNode"` automatically

---

## Project structure

```
Final Project/
├── RoomHubProject/      # Central hub firmware
├── EnvironmentalNode/   # Temp, humidity, motion, sound
├── CircadianNode/       # Curtain + lighting control
├── PersonalNode/        # Heart rate + LoRaWAN
├── BuildingNode/        # LoRa gateway receiver
└── Backend/             # Server-side (if applicable)
```
