/**
 * Import function triggers from their respective submodules:
 *
 * import {onCall} from "firebase-functions/v2/https";
 * import {onDocumentWritten} from "firebase-functions/v2/firestore";
 *
 * See a full list of supported triggers at https://firebase.google.com/docs/functions
 */

import {setGlobalOptions} from "firebase-functions";
import {onRequest} from "firebase-functions/v2/https";
import * as logger from "firebase-functions/logger";
import {defineSecret} from "firebase-functions/params";

const ROOM_SECRET = defineSecret("ROOM_SECRET");

interface SensorReadingBody {
  sensorId: string;
  value: string;
}

enum SensorType {
  TEMPERATURE = 0x01,
  HUMIDITY = 0x02,
  HEARTRATE = 0x03,
}

const sensorTypeToString = (sensorType: SensorType) => {
  if(sensorType == SensorType.TEMPERATURE) return "TEMPERATURE"
  if(sensorType == SensorType.HUMIDITY) return "HUMIDITY"
  if(sensorType == SensorType.HEARTRATE) return "HEARTRATE"
  return "UNKNOWN"
}


// ByteArray structure: [deviceId][sequence][sensorEntryCount][sensorEntry]
const hexStrToByteArr = (hexStr: String) => {
 return Uint8Array.from(
  hexStr.match(/.{1,2}/g)!.map(byte => parseInt(byte, 16))
);
}

const decodePayload = (byteArrayHexString: String) => {
  const byteArray = hexStrToByteArr(byteArrayHexString)
  let sensorEntryArr: SensorReadingBody[] = []
  let byteCounter = 0
  let currentSensorReading: SensorReadingBody = {
    sensorId: "",
    value: "",
  }

  byteArray.subarray(3).forEach((byte, index) => {
    if(byteCounter == 0) {
      currentSensorReading.sensorId = String(byte)
    }
    const isTemp = currentSensorReading.sensorId == "1"
    
    if(byteCounter == 1) {
      if(isTemp) currentSensorReading.value += String(byte)
    }
    if(byteCounter == 2) {
      if(isTemp) currentSensorReading.value += String(byte)
    }
    if(byteCounter == 3) {
      let sensorValueAsString = String(parseInt(currentSensorReading.value, 16))
      const formatted = sensorValueAsString.slice(0, 2) + "." + sensorValueAsString.slice(2)
      currentSensorReading.value = formatted
    }
      
    byteCounter++
    if(byteCounter == 4) {
      sensorEntryArr.push(currentSensorReading)
      byteCounter = 0
    }
  })
  return {
    deviceId: byteArray[0],
    sequence: byteArray[1],
    sensorEntryCount: byteArray[2], 
    sensorEntries: sensorEntryArr
  }
}

const test = decodePayload("0101020102092901020947")




const {createSensorReading} = require("../src/dataconnect-admin-generated/index.cjs.js") as {
  createSensorReading: (args: {
    sensorId: string;
    value: number;
    unit?: string;
    description?: string;
    timestamp: string;
  }) => Promise<unknown>;
};

setGlobalOptions({ maxInstances: 10 });

export const writeSensorReading = onRequest({secrets: [ROOM_SECRET]}, async (req, res) => {
  try {
    const auth = req.headers.authorization ?? "";
    const token = auth.replace(/^Bearer\s+/i, "");

    if (token != ROOM_SECRET.value()) {
      res.status(401).send("Unauthorized");
      return;
    }

    if (req.method !== "POST") {
      res.status(405).send("Method not allowed");
      return;
    }

    // Type the request body according to your schema
    const body = req.body as SensorReadingBody;
    
    // Validate required fields
    if (!body.sensorId || body.value === undefined) {
      res.status(400).json({ error: "Missing required fields: sensorId, value" });
      return;
    }

    // Type-safe mutation to insert sensor reading
    const result = await createSensorReading({
      sensorId: body.sensorId,
      value: body.value,
      unit: body.unit,
      description: body.description,
      timestamp: new Date().toISOString()
    });

    logger.info(`Sensor reading recorded`, {
      structuredData: true,
      sensorId: body.sensorId,
      value: body.value
    });

    res.json({ success: true, data: result });
  } catch (error) {
    if(error instanceof Error) {
      logger.error("Error recording sensor reading", error);
      res.status(500).json({ error: error.message });
    }
  }
});

export const helloWorld = onRequest({secrets: [ROOM_SECRET]},(req, res) => {
  const auth = req.headers.authorization ?? "";
  const token = auth.replace(/^Bearer\s+/i, "");

  if (token != ROOM_SECRET.value()) {
    res.status(401).send("Unauthorized");
    return;
  }

  logger.info("Hello from ESP32 👋");
  res.status(200).send("Hello ESP32");
});

