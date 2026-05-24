// seedFirestore.js
const admin = require("firebase-admin");
admin.initializeApp({
  projectId: "iot-sleeping",
  credential: admin.credential.applicationDefault(),
});
const db = admin.firestore();
async function seed() {
  const now = admin.firestore.Timestamp.fromDate(new Date("2026-05-19T19:39:32.589Z"));
  const buildingRef = db.collection("buildings").doc("building-demo-01");
  const roomRef = db.collection("rooms").doc("room-101");
  const collectionRef = db.collection("sensorCollections").doc("room-101-node");
  const sensorsRef = db.collection("sensors");
  await buildingRef.set({
    name: "Demo Medical Hotel",
    address: "Demo Address 1",
    description: "Demo building for Lorca sleep monitoring prototype",
    createdAt: now,
  });
  await roomRef.set({
    buildingId: buildingRef.id,
    buildingName: "Demo Medical Hotel",
    name: "Room 101",
    floor: 1,
    description: "Demo patient room",
    createdAt: now,
  });
  await collectionRef.set({
    buildingId: buildingRef.id,
    roomId: roomRef.id,
    roomName: "Room 101",
    name: "Room 101 Node",
    deviceId: "node-01",
    description: "ESP32 room node collecting environmental and patient telemetry",
    createdAt: now,
  });
  const sensors = {
    temp: {
      id: "sensor-temp-room-101",
      name: "Room 101 Temperature",
      type: "TEMPERATURE",
      unit: "°C",
      serialNumber: "TEMP-ROOM101-001",
    },
    humidity: {
      id: "sensor-humidity-room-101",
      name: "Room 101 Humidity",
      type: "HUMIDITY",
      unit: "%",
      serialNumber: "HUM-ROOM101-001",
    },
    heart: {
      id: "sensor-heartrate-room-101",
      name: "Room 101 Heart Rate",
      type: "HEARTRATE",
      unit: "bpm",
      serialNumber: "HR-ROOM101-001",
    },
    sleep: {
      id: "sensor-sleep-event-room-101",
      name: "Room 101 Sleep Events",
      type: "SLEEP_EVENT",
      unit: "event",
      serialNumber: "SLEEP-ROOM101-001",
    },
  };
  for (const sensor of Object.values(sensors)) {
    await sensorsRef.doc(sensor.id).set({
      ...sensor,
      buildingId: buildingRef.id,
      roomId: roomRef.id,
      sensorCollectionId: collectionRef.id,
      locationDetails: "Room 101",
      description: `${sensor.name} demo sensor`,
      createdAt: now,
    });
  }
  const readings = [
    ["temp", 22.8, "2026-05-19T19:00:00Z"],
    ["humidity", 41.2, "2026-05-19T19:01:00Z"],
    ["heart", 68, "2026-05-19T19:02:00Z"],
    ["sleep", 0, "2026-05-19T19:03:00Z"],
    ["temp", 23.1, "2026-05-19T19:04:00Z"],
    ["humidity", 42.0, "2026-05-19T19:05:00Z"],
    ["heart", 72, "2026-05-19T19:06:00Z"],
    ["sleep", 1, "2026-05-19T19:07:00Z"],
    ["temp", 22.9, "2026-05-19T19:08:00Z"],
    ["heart", 70, "2026-05-19T19:09:00Z"],
  ];
  const batch = db.batch();
  readings.forEach(([sensorKey, value, timestamp], index) => {
    const sensor = sensors[sensorKey];
    const readingRef = db.collection("sensorReadings").doc(`reading-${String(index + 1).padStart(2, "0")}`);
    batch.set(readingRef, {
      buildingId: buildingRef.id,
      buildingName: "Demo Medical Hotel",
      roomId: roomRef.id,
      roomName: "Room 101",
      sensorCollectionId: collectionRef.id,
      deviceId: "node-01",
      sensorId: sensor.id,
      sensorName: sensor.name,
      sensorType: sensor.type,
      value,
      unit: sensor.unit,
      timestamp: admin.firestore.Timestamp.fromDate(new Date(timestamp)),
      createdAt: admin.firestore.FieldValue.serverTimestamp(),
    });
  });

  await batch.commit();
  console.log("Seeded Firestore demo data.");
}

seed().catch((err) => {
  console.error(err);
  process.exit(1);
});