import { validateAdminArgs } from 'firebase-admin/data-connect';

export const SensorType = {
  TEMPERATURE: "TEMPERATURE",
  HUMIDITY: "HUMIDITY",
  HEARTRATE: "HEARTRATE",
  BRIGHTNESS: "BRIGHTNESS",
  ACCELEROMETER: "ACCELEROMETER",
  MOTION: "MOTION",
  SOUND: "SOUND",
}

export const connectorConfig = {
  connector: 'example',
  serviceId: 'iot-sleeping-service',
  location: 'europe-west2'
};

export function createSensor(dcOrVarsOrOptions, varsOrOptions, options) {
  const { dc: dcInstance, vars: inputVars, options: inputOpts} = validateAdminArgs(connectorConfig, dcOrVarsOrOptions, varsOrOptions, options, true, true);
  dcInstance.useGen(true);
  return dcInstance.executeMutation('CreateSensor', inputVars, inputOpts);
}

export function createSensorReading(dcOrVarsOrOptions, varsOrOptions, options) {
  const { dc: dcInstance, vars: inputVars, options: inputOpts} = validateAdminArgs(connectorConfig, dcOrVarsOrOptions, varsOrOptions, options, true, true);
  dcInstance.useGen(true);
  return dcInstance.executeMutation('CreateSensorReading', inputVars, inputOpts);
}

export function listSensors(dcOrOptions, options) {
  const { dc: dcInstance, options: inputOpts} = validateAdminArgs(connectorConfig, dcOrOptions, options, undefined);
  dcInstance.useGen(true);
  return dcInstance.executeQuery('ListSensors', undefined, inputOpts);
}

