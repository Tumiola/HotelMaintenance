/******************************************************************************
 * File:        index.ts
 * Project:     LORCA - LoRa Medical Hotel Monitoring System
 *
 * Description:
 * Cloud Function backend responsible for:
 *   - Receiving LoRa uplink payloads over HTTPS
 *   - Receiving cibicom/LORIOT HTTP Push webhooks
 *   - Decoding compact binary LoRa payloads
 *   - Saving decoded sensor readings to Firestore
 *
 * Author:      Askur Hugi - s251904
 * Group:       Group 6
 * Course:      34346 - Networking technologies and application
 *                      development for Internet of Things (IoT)
 * Institution: Technical University of Denmark (DTU)
 *
 * Date:        2026-05-24
 ******************************************************************************/


import {setGlobalOptions} from "firebase-functions";
import {onRequest} from "firebase-functions/v2/https";
import * as logger from "firebase-functions/logger";
import {defineSecret} from "firebase-functions/params";
import {initializeApp} from "firebase-admin/app";
import {getFirestore, Timestamp, FieldValue} from "firebase-admin/firestore";

// Initializes Firebase Admin SDK.
initializeApp();

// Sets up Firestore and the shared room secret.
const db = getFirestore();
const ROOM_SECRET = defineSecret("ROOM_SECRET");

// Limits the amount of function instances that can run at the same time.
setGlobalOptions({maxInstances: 10});

// Defines the sensor type bytes used in the LoRa payload.
enum SensorType {
  TEMPERATURE = 0x01,
  HUMIDITY = 0x02,
  HEARTRATE = 0x03,
  SLEEP_EVENT = 0x04,
}

// Maps sensor type bytes to Firestore sensor metadata.
const SENSOR_METADATA = {
  [SensorType.TEMPERATURE]: {
    sensorId: "sensor-temp-room-101",
    sensorName: "Room 101 Temperature",
    sensorType: "TEMPERATURE",
    unit: "°C",
  },
  [SensorType.HUMIDITY]: {
    sensorId: "sensor-humidity-room-101",
    sensorName: "Room 101 Humidity",
    sensorType: "HUMIDITY",
    unit: "%",
  },
  [SensorType.HEARTRATE]: {
    sensorId: "sensor-heartrate-room-101",
    sensorName: "Room 101 Heart Rate",
    sensorType: "HEARTRATE",
    unit: "bpm",
  },
  [SensorType.SLEEP_EVENT]: {
    sensorId: "sensor-sleep-event-room-101",
    sensorName: "Room 101 Sleep Events",
    sensorType: "SLEEP_EVENT",
    unit: "event",
  },
} as const;

// Stores the static Firestore context for proof of concept.
const FIRESTORE_CONTEXT = {
  buildingId: "building-demo-01",
  buildingName: "Demo Medical Hotel",
  roomId: "room-101",
  roomName: "Room 101",
  sensorCollectionId: "room-101-node",
};

const DOWNLINK_DOC = "latest";

// Gets the bearer token from the Authorization header.
const getBearerToken = (authorizationHeader: string | undefined): string => {
  return (authorizationHeader ?? "").replace(/^Bearer\s+/i, "");
};

// Checks if the request has the correct room secret.
const isAuthorized = (authorizationHeader: string | undefined): boolean => {
  return getBearerToken(authorizationHeader) === ROOM_SECRET.value();
};

// Converts a hex string payload to a byte array.
const hexStringToByteArray = (hexString: string): Uint8Array => {
  const normalized = hexString.trim().replace(/^0x/i, "").replace(/\s+/g, "");

  // Checks if the string contains anything else than hex characters.
  if (!/^[0-9a-fA-F]*$/.test(normalized)) {
    throw new Error("Payload hex string contains non-hex characters");
  }

  // Checks if possibble to split into full bytes.
  if (normalized.length === 0 || normalized.length % 2 !== 0) {
    throw new Error("Payload hex string must contain an even number of hex characters");
  }

  const bytes = normalized.match(/.{1,2}/g) ?? [];
  return Uint8Array.from(bytes.map((byte) => parseInt(byte, 16)));
};

// Converts a base64 to a byte array.
const base64ToByteArray = (base64String: string): Uint8Array => {
  return Uint8Array.from(Buffer.from(base64String, "base64"));
};

// Reads a signed 16-bit big-endian value from two bytes.
const readInt16BE = (highByte: number, lowByte: number): number => {
  const unsigned = (highByte << 8) | lowByte;
  return unsigned & 0x8000 ? unsigned - 0x10000 : unsigned;
};

// Decodes the raw sensor value from type.
const decodeSensorValue = (sensorType: SensorType, rawValue: number): number => {
  switch (sensorType) {
  case SensorType.TEMPERATURE:
  case SensorType.HUMIDITY:
    return rawValue / 100;
  case SensorType.HEARTRATE:
  case SensorType.SLEEP_EVENT:
    return rawValue;
  default:
    throw new Error(`Unknown sensor type: ${sensorType}`);
  }
};

// Defines one decoded sensor reading from the LoRa payload.
type DecodedSensorEntry = {
  entryIndex: number;
  sensorTypeByte: number;
  sensorId: string;
  sensorName: string;
  sensorType: string;
  rawValue: number;
  value: number;
  unit: string;
};

// Defines the full decoded LoRa payload.
type DecodedPayload = {
  deviceIdByte: number;
  sequence: number;
  sensorEntryCount: number;
  sensorEntries: DecodedSensorEntry[];
};

// Decodes the compact binary LoRa payload into sensor entries.
const decodePayload = (byteArray: Uint8Array): DecodedPayload => {
  // Checks if the payload contains the required header bytes.
  if (byteArray.length < 3) {
    throw new Error("Payload must contain at least 3 header bytes");
  }

  const deviceIdByte = byteArray[0];
  const sequence = byteArray[1];
  const sensorEntryCount = byteArray[2];

  const sensorEntries: DecodedSensorEntry[] = [];
  let index = 3;

  // Decodes every sensor entry in the payload.
  for (let entryIndex = 0; entryIndex < sensorEntryCount; entryIndex++) {
    // Checks if the current entry has all required bytes.
    if (index + 4 > byteArray.length) {
      throw new Error(`Payload ended early while decoding entry ${entryIndex}`);
    }

    const sensorTypeByte = byteArray[index] as SensorType;
    const length = byteArray[index + 1];

    // Checks if the payload uses the expected 2-byte sensor value format.
    if (length !== 2) {
      throw new Error(`Unsupported sensor value length ${length}. Expected 2.`);
    }

    const metadata = SENSOR_METADATA[sensorTypeByte];

    // Checks if the sensor type byte is registered in "SENSOR_METADATA".
    if (!metadata) {
      throw new Error(`Unknown sensor type byte: ${sensorTypeByte}`);
    }

    const rawValue = readInt16BE(byteArray[index + 2], byteArray[index + 3]);
    const value = decodeSensorValue(sensorTypeByte, rawValue);

    // Adds the decoded sensor entry to "sensorEntries".
    sensorEntries.push({
      entryIndex,
      sensorTypeByte,
      sensorId: metadata.sensorId,
      sensorName: metadata.sensorName,
      sensorType: metadata.sensorType,
      rawValue,
      value,
      unit: metadata.unit,
    });

    index += 4;
  }

  // Checks if the payload contains extra bytes after the declared entries.
  if (index !== byteArray.length) {
    throw new Error(`Payload has ${byteArray.length - index} unused trailing byte(s)`);
  }

  return {
    deviceIdByte,
    sequence,
    sensorEntryCount,
    sensorEntries,
  };
};

// Parses the received timestamp from the webhook body.
const parseTimestamp = (body: Record<string, unknown>): Timestamp => {
  const possibleTimestamp =
    body.receivedAt ??
    body.timestamp ??
    body.time ??
    body.ts;

  // Uses ISO/date-string timestamps if supplied.
  if (typeof possibleTimestamp === "string") {
    return Timestamp.fromDate(new Date(possibleTimestamp));
  }

  // Uses numeric timestamps as seconds or milliseconds.
  if (typeof possibleTimestamp === "number") {
    const millis = possibleTimestamp > 10_000_000_000 ?
      possibleTimestamp :
      possibleTimestamp * 1000;

    return Timestamp.fromMillis(millis);
  }

  // Falls back to server time if no timestamp was supplied.
  return Timestamp.now();
};

// Searches an object recursively for the first matching string key.
const findNestedString = (
  value: unknown,
  keys: string[]
): string | undefined => {
  if (!value || typeof value !== "object") {
    return undefined;
  }

  const record = value as Record<string, unknown>;

  // Checks the current object level first.
  for (const key of keys) {
    const directValue = record[key];

    if (typeof directValue === "string") {
      return directValue;
    }
  }

  // Checks nested objects if the value was not found at the current level.
  for (const nestedValue of Object.values(record)) {
    const found = findNestedString(nestedValue, keys);

    if (found) {
      return found;
    }
  }

  return undefined;
};

// Extracts the lora payload from esp gateway or cibicomwebhook bodies.
const extractPayload = (body: Record<string, unknown>): Uint8Array => {
  // Checks for payload fields that usually contain hex strings.
  const hexPayload = findNestedString(body, [
    "payloadHex",
    "payload_hex",
    "data",
    "hex",
    "payload_hex_string",
  ]);

  if (hexPayload && /^[0-9a-fA-F\s]+$/.test(hexPayload)) {
    return hexStringToByteArray(hexPayload);
  }

  // Checks for payload fields that usually contain base64 strings.
  const base64Payload = findNestedString(body, [
    "payloadBase64",
    "payload",
    "frm_payload",
    "data_base64",
  ]);

  if (base64Payload) {
    return base64ToByteArray(base64Payload);
  }

  throw new Error(
    "No usable payload found. Expected payloadHex/data hex or payloadBase64/payload/frm_payload base64."
  );
};

// Extracts an external device id from the webhook body or falls back to the payload device byte.
const extractExternalDeviceId = (
  body: Record<string, unknown>,
  decoded: DecodedPayload
): string => {
  const deviceId = findNestedString(body, [
    "gatewayId",
    "deviceId",
    "devEUI",
    "devEui",
    "deveui",
    "EUI",
    "eui",
  ]);

  if (deviceId) {
    return deviceId;
  }

  return `node-${String(decoded.deviceIdByte).padStart(2, "0")}`;
};

// Decodes an uplink payload and saves all sensor readings to "sensorReadings".
const processUplinkPayload = async (
  body: Record<string, unknown>,
  source: "esp32-gateway" | "cibicom-webhook"
) => {
  const byteArray = extractPayload(body);
  const decoded = decodePayload(byteArray);
  const timestamp = parseTimestamp(body);
  const deviceId = extractExternalDeviceId(body, decoded);

  const batch = db.batch();

  // Create firestore document for each decoded sensor entry.
  decoded.sensorEntries.forEach((entry) => {
    const readingRef = db.collection("sensorReadings").doc();

    batch.set(readingRef, {
      ...FIRESTORE_CONTEXT,

      deviceId,
      deviceIdByte: decoded.deviceIdByte,
      sequence: decoded.sequence,

      sensorId: entry.sensorId,
      sensorName: entry.sensorName,
      sensorType: entry.sensorType,

      value: entry.value,
      rawValue: entry.rawValue,
      unit: entry.unit,

      timestamp,
      createdAt: FieldValue.serverTimestamp(),

      source,
      webhookBody: body,
    });
  });

  // Writes all decoded sensor readings at once.
  await batch.commit();

  return {
    deviceId,
    sequence: decoded.sequence,
    inserted: decoded.sensorEntries.length,
    decoded,
  };
};

//-------------------------------------------------gatewayUplink-----------------------------------------------------------

// Receives uplinks from the esp building gateway.
export const gatewayUplink = onRequest(
  {secrets: [ROOM_SECRET]},
  async (req, res) => {
    try {
      // Only allows post requests.
      if (req.method !== "POST") {
        res.status(405).send("Method not allowed");
        return;
      }

      // Checks if the esp gateway request is authorized.
      if (!isAuthorized(req.headers.authorization)) {
        res.status(401).send("Unauthorized");
        return;
      }

      const body = req.body as Record<string, unknown>;

      logger.info("ESP32 gateway uplink received", {
        structuredData: true,
        body,
      });

      // Decodes the payload and saves the sensor readings to firestore.
      const result = await processUplinkPayload(body, "esp32-gateway");

      res.status(200).json({
        success: true,
        ...result,
      });
    } catch (error) {
      logger.error("Failed to process ESP32 gateway uplink", error);

      res.status(500).json({
        success: false,
        error: error instanceof Error ? error.message : String(error),
      });
    }
  }
);

//-------------------------------------------------helloWorld-----------------------------------------------------------

// Receives uplinks from cibicom HTTP push.
// Keep the exported name as "helloWorld" because cibicom already setup for that name.
export const helloWorld = onRequest(
  {secrets: [ROOM_SECRET]},
  async (req, res) => {
    try {
      // Only allows POST requests.
      if (req.method !== "POST") {
        res.status(405).send("Method not allowed");
        return;
      }

      // Checks if the cibicom request is authorized.
      if (!isAuthorized(req.headers.authorization)) {
        res.status(401).send("Unauthorized");
        return;
      }

      const body = req.body as Record<string, unknown>;

      logger.info("cibicom webhook received", {
        structuredData: true,
        body,
      });

      // Decodes the payload and saves the sensor readings to firestore.
      const result = await processUplinkPayload(body, "cibicom-webhook");

      logger.info("cibicom payload written to Firestore", {
        structuredData: true,
        ...result,
      });

      res.status(200).json({
        success: true,
        ...result,
      });
    } catch (error) {
      logger.error("Failed to process cibicom webhook", error);

      res.status(500).json({
        success: false,
        error: error instanceof Error ? error.message : String(error),
      });
    }
  }
);

//-------------------------------------------------createDownlink-----------------------------------------------------------

// Saves the latest downlink command.
export const createDownlink = onRequest(
  {secrets: [ROOM_SECRET]},
  async (req, res) => {
    try {
      // Only allows POST requests.
      if (req.method !== "POST") {
        res.status(405).send("Method not allowed");
        return;
      }

      // Checks if the request is authorized.
      if (!isAuthorized(req.headers.authorization)) {
        res.status(401).send("Unauthorized");
        return;
      }

      const commandHex = String(req.body.commandHex ?? "").trim().toUpperCase();

      // Checks if command is 3 bytes.
      if (!/^[0-9A-F]{6}$/.test(commandHex)) {
        res.status(400).send("Bad commandHex");
        return;
      }

      // Saves command to firestore.
      await db.collection("downlinkCommands").doc(DOWNLINK_DOC).set({
        commandHex,
        pending: true,
        createdAt: FieldValue.serverTimestamp(),
      });

      res.status(200).send("OK");
    } catch (error) {
      logger.error("Failed to create downlink", error);
      res.status(500).send("ERROR");
    }
  }
);

//-------------------------------------------------pollDownlink-----------------------------------------------------------

// Lets the building node check if a command is waiting.
export const pollDownlink = onRequest(
  {secrets: [ROOM_SECRET]},
  async (req, res) => {
    try {
      // Only allows GET requests.
      if (req.method !== "GET") {
        res.status(405).send("Method not allowed");
        return;
      }

      // Checks if the building node is authorized.
      if (!isAuthorized(req.headers.authorization)) {
        res.status(401).send("Unauthorized");
        return;
      }

      const ref = db.collection("downlinkCommands").doc(DOWNLINK_DOC);
      const doc = await ref.get();

      // Returns nothing useful if there is no command.
      if (!doc.exists || doc.data()?.pending !== true) {
        res.status(200).send("NONE");
        return;
      }

      const commandHex = String(doc.data()?.commandHex ?? "").trim().toUpperCase();

      // Marks the command as used.
      await ref.update({
        pending: false,
        sentAt: FieldValue.serverTimestamp(),
      });

      res.status(200).send(commandHex);
    } catch (error) {
      logger.error("Failed to poll downlink", error);
      res.status(500).send("ERROR");
    }
  }
);