const { queryRef, executeQuery, validateArgsWithOptions, mutationRef, executeMutation, validateArgs, makeMemoryCacheProvider } = require('firebase/data-connect');

const SensorType = {
  TEMPERATURE: "TEMPERATURE",
  HUMIDITY: "HUMIDITY",
  HEARTRATE: "HEARTRATE",
  BRIGHTNESS: "BRIGHTNESS",
  ACCELEROMETER: "ACCELEROMETER",
  MOTION: "MOTION",
  SOUND: "SOUND",
  SLEEP_EVENT: "SLEEP_EVENT",
}
exports.SensorType = SensorType;

const connectorConfig = {
  connector: 'example',
  service: 'iot-sleeping-service',
  location: 'europe-west2'
};
exports.connectorConfig = connectorConfig;
const dataConnectSettings = {
  cacheSettings: {
    cacheProvider: makeMemoryCacheProvider(),
    maxAgeSeconds: 5
  }
};
exports.dataConnectSettings = dataConnectSettings;

const createSensorRef = (dcOrVars, vars) => {
  const { dc: dcInstance, vars: inputVars} = validateArgs(connectorConfig, dcOrVars, vars, true);
  dcInstance._useGeneratedSdk();
  return mutationRef(dcInstance, 'CreateSensor', inputVars);
}
createSensorRef.operationName = 'CreateSensor';
exports.createSensorRef = createSensorRef;

exports.createSensor = function createSensor(dcOrVars, vars) {
  const { dc: dcInstance, vars: inputVars } = validateArgs(connectorConfig, dcOrVars, vars, true);
  return executeMutation(createSensorRef(dcInstance, inputVars));
}
;

const createSensorReadingRef = (dcOrVars, vars) => {
  const { dc: dcInstance, vars: inputVars} = validateArgs(connectorConfig, dcOrVars, vars, true);
  dcInstance._useGeneratedSdk();
  return mutationRef(dcInstance, 'CreateSensorReading', inputVars);
}
createSensorReadingRef.operationName = 'CreateSensorReading';
exports.createSensorReadingRef = createSensorReadingRef;

exports.createSensorReading = function createSensorReading(dcOrVars, vars) {
  const { dc: dcInstance, vars: inputVars } = validateArgs(connectorConfig, dcOrVars, vars, true);
  return executeMutation(createSensorReadingRef(dcInstance, inputVars));
}
;

const listSensorsRef = (dc) => {
  const { dc: dcInstance} = validateArgs(connectorConfig, dc, undefined);
  dcInstance._useGeneratedSdk();
  return queryRef(dcInstance, 'ListSensors');
}
listSensorsRef.operationName = 'ListSensors';
exports.listSensorsRef = listSensorsRef;

exports.listSensors = function listSensors(dcOrOptions, options) {
  
  const { dc: dcInstance, vars: inputVars, options: inputOpts } = validateArgsWithOptions(connectorConfig, dcOrOptions, options, undefined,false, false);
  return executeQuery(listSensorsRef(dcInstance, inputVars), inputOpts && inputOpts.fetchPolicy);
}
;

const listSensorReadingsRef = (dc) => {
  const { dc: dcInstance} = validateArgs(connectorConfig, dc, undefined);
  dcInstance._useGeneratedSdk();
  return queryRef(dcInstance, 'ListSensorReadings');
}
listSensorReadingsRef.operationName = 'ListSensorReadings';
exports.listSensorReadingsRef = listSensorReadingsRef;

exports.listSensorReadings = function listSensorReadings(dcOrOptions, options) {
  
  const { dc: dcInstance, vars: inputVars, options: inputOpts } = validateArgsWithOptions(connectorConfig, dcOrOptions, options, undefined,false, false);
  return executeQuery(listSensorReadingsRef(dcInstance, inputVars), inputOpts && inputOpts.fetchPolicy);
}
;
