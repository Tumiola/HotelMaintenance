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

interface SensorReadingBody {
  sensorId: string;
  value: number;
  unit?: string;
  description?: string;
}

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

export const writeSensorReading = onRequest(async (req, res) => {
  try {
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

export const helloWorld = onRequest((req, res) => {
  logger.info("Hello from ESP32 👋");

  res.status(200).send("Hello ESP32");
});