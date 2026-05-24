import { queryRef, executeQuery, validateArgsWithOptions, mutationRef, executeMutation, validateArgs, makeMemoryCacheProvider } from 'firebase/data-connect';

export const SensorType = {
  TEMPERATURE: "TEMPERATURE",
  HUMIDITY: "HUMIDITY",
  HEARTRATE: "HEARTRATE",
  BRIGHTNESS: "BRIGHTNESS",
  ACCELEROMETER: "ACCELEROMETER",
  MOTION: "MOTION",
  SOUND: "SOUND",
  SLEEP_EVENT: "SLEEP_EVENT",
}

export const connectorConfig = {
  connector: 'example',
  service: 'iot-sleeping-service',
  location: 'europe-west2'
};
export const dataConnectSettings = {
  cacheSettings: {
    cacheProvider: makeMemoryCacheProvider(),
    maxAgeSeconds: 5
  }
};
export const createSensorRef = (dcOrVars, vars) => {
  const { dc: dcInstance, vars: inputVars} = validateArgs(connectorConfig, dcOrVars, vars, true);
  dcInstance._useGeneratedSdk();
  return mutationRef(dcInstance, 'CreateSensor', inputVars);
}
createSensorRef.operationName = 'CreateSensor';

export function createSensor(dcOrVars, vars) {
  const { dc: dcInstance, vars: inputVars } = validateArgs(connectorConfig, dcOrVars, vars, true);
  return executeMutation(createSensorRef(dcInstance, inputVars));
}

export const createSensorReadingRef = (dcOrVars, vars) => {
  const { dc: dcInstance, vars: inputVars} = validateArgs(connectorConfig, dcOrVars, vars, true);
  dcInstance._useGeneratedSdk();
  return mutationRef(dcInstance, 'CreateSensorReading', inputVars);
}
createSensorReadingRef.operationName = 'CreateSensorReading';

export function createSensorReading(dcOrVars, vars) {
  const { dc: dcInstance, vars: inputVars } = validateArgs(connectorConfig, dcOrVars, vars, true);
  return executeMutation(createSensorReadingRef(dcInstance, inputVars));
}

export const listSensorsRef = (dc) => {
  const { dc: dcInstance} = validateArgs(connectorConfig, dc, undefined);
  dcInstance._useGeneratedSdk();
  return queryRef(dcInstance, 'ListSensors');
}
listSensorsRef.operationName = 'ListSensors';

export function listSensors(dcOrOptions, options) {
  
  const { dc: dcInstance, vars: inputVars, options: inputOpts } = validateArgsWithOptions(connectorConfig, dcOrOptions, options, undefined,false, false);
  return executeQuery(listSensorsRef(dcInstance, inputVars), inputOpts && inputOpts.fetchPolicy);
}

export const listSensorReadingsRef = (dc) => {
  const { dc: dcInstance} = validateArgs(connectorConfig, dc, undefined);
  dcInstance._useGeneratedSdk();
  return queryRef(dcInstance, 'ListSensorReadings');
}
listSensorReadingsRef.operationName = 'ListSensorReadings';

export function listSensorReadings(dcOrOptions, options) {
  
  const { dc: dcInstance, vars: inputVars, options: inputOpts } = validateArgsWithOptions(connectorConfig, dcOrOptions, options, undefined,false, false);
  return executeQuery(listSensorReadingsRef(dcInstance, inputVars), inputOpts && inputOpts.fetchPolicy);
}

