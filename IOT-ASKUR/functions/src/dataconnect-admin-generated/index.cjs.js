const { validateAdminArgs } = require('firebase-admin/data-connect');

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
  serviceId: 'iot-sleeping-service',
  location: 'europe-west2'
};
exports.connectorConfig = connectorConfig;

function createSensor(dcOrVarsOrOptions, varsOrOptions, options) {
  const { dc: dcInstance, vars: inputVars, options: inputOpts} = validateAdminArgs(connectorConfig, dcOrVarsOrOptions, varsOrOptions, options, true, true);
  dcInstance.useGen(true);
  return dcInstance.executeMutation('CreateSensor', inputVars, inputOpts);
}
exports.createSensor = createSensor;

function createSensorReading(dcOrVarsOrOptions, varsOrOptions, options) {
  const { dc: dcInstance, vars: inputVars, options: inputOpts} = validateAdminArgs(connectorConfig, dcOrVarsOrOptions, varsOrOptions, options, true, true);
  dcInstance.useGen(true);
  return dcInstance.executeMutation('CreateSensorReading', inputVars, inputOpts);
}
exports.createSensorReading = createSensorReading;

function listSensors(dcOrOptions, options) {
  const { dc: dcInstance, options: inputOpts} = validateAdminArgs(connectorConfig, dcOrOptions, options, undefined);
  dcInstance.useGen(true);
  return dcInstance.executeQuery('ListSensors', undefined, inputOpts);
}
exports.listSensors = listSensors;

function listSensorReadings(dcOrOptions, options) {
  const { dc: dcInstance, options: inputOpts} = validateAdminArgs(connectorConfig, dcOrOptions, options, undefined);
  dcInstance.useGen(true);
  return dcInstance.executeQuery('ListSensorReadings', undefined, inputOpts);
}
exports.listSensorReadings = listSensorReadings;

