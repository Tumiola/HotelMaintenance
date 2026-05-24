import { ConnectorConfig, DataConnect, OperationOptions, ExecuteOperationResponse } from 'firebase-admin/data-connect';

export const connectorConfig: ConnectorConfig;

export type TimestampString = string;
export type UUIDString = string;
export type Int64String = string;
export type DateString = string;

export enum SensorType {
  TEMPERATURE = "TEMPERATURE",
  HUMIDITY = "HUMIDITY",
  HEARTRATE = "HEARTRATE",
  BRIGHTNESS = "BRIGHTNESS",
  ACCELEROMETER = "ACCELEROMETER",
  MOTION = "MOTION",
  SOUND = "SOUND",
  SLEEP_EVENT = "SLEEP_EVENT",
}

export interface Building_Key {
  id: UUIDString;
  __typename?: 'Building_Key';
}

export interface CreateSensorData {
  sensor_insert: Sensor_Key;
}

export interface CreateSensorReadingData {
  sensorReading_insert: SensorReading_Key;
}

export interface CreateSensorReadingVariables {
  sensorId: UUIDString;
  value: number;
  unit?: string | null;
  description?: string | null;
  timestamp: TimestampString;
}

export interface CreateSensorVariables {
  sensorCollectionId: UUIDString;
  name: string;
  type: SensorType;
  unit: string;
  description?: string | null;
  serialNumber?: string | null;
  locationDetails?: string | null;
  createdAt: TimestampString;
}

export interface ListSensorReadingsData {
  sensorReadings: ({
    id: UUIDString;
    value: number;
    unit?: string | null;
    description?: string | null;
    timestamp: TimestampString;
    sensor: {
      id: UUIDString;
      name: string;
      type: SensorType;
      unit: string;
      serialNumber?: string | null;
      locationDetails?: string | null;
      sensorCollection: {
        id: UUIDString;
        name: string;
        room: {
          id: UUIDString;
          name: string;
          floor?: number | null;
          building: {
            id: UUIDString;
            name: string;
            address?: string | null;
          } & Building_Key;
        } & Room_Key;
      } & SensorCollection_Key;
    } & Sensor_Key;
  } & SensorReading_Key)[];
}

export interface ListSensorsData {
  sensors: ({
    id: UUIDString;
    name: string;
    type: SensorType;
    unit: string;
  } & Sensor_Key)[];
}

export interface Room_Key {
  id: UUIDString;
  __typename?: 'Room_Key';
}

export interface SensorCollection_Key {
  id: UUIDString;
  __typename?: 'SensorCollection_Key';
}

export interface SensorReading_Key {
  id: UUIDString;
  __typename?: 'SensorReading_Key';
}

export interface Sensor_Key {
  id: UUIDString;
  __typename?: 'Sensor_Key';
}

/** Generated Node Admin SDK operation action function for the 'CreateSensor' Mutation. Allow users to execute without passing in DataConnect. */
export function createSensor(dc: DataConnect, vars: CreateSensorVariables, options?: OperationOptions): Promise<ExecuteOperationResponse<CreateSensorData>>;
/** Generated Node Admin SDK operation action function for the 'CreateSensor' Mutation. Allow users to pass in custom DataConnect instances. */
export function createSensor(vars: CreateSensorVariables, options?: OperationOptions): Promise<ExecuteOperationResponse<CreateSensorData>>;

/** Generated Node Admin SDK operation action function for the 'CreateSensorReading' Mutation. Allow users to execute without passing in DataConnect. */
export function createSensorReading(dc: DataConnect, vars: CreateSensorReadingVariables, options?: OperationOptions): Promise<ExecuteOperationResponse<CreateSensorReadingData>>;
/** Generated Node Admin SDK operation action function for the 'CreateSensorReading' Mutation. Allow users to pass in custom DataConnect instances. */
export function createSensorReading(vars: CreateSensorReadingVariables, options?: OperationOptions): Promise<ExecuteOperationResponse<CreateSensorReadingData>>;

/** Generated Node Admin SDK operation action function for the 'ListSensors' Query. Allow users to execute without passing in DataConnect. */
export function listSensors(dc: DataConnect, options?: OperationOptions): Promise<ExecuteOperationResponse<ListSensorsData>>;
/** Generated Node Admin SDK operation action function for the 'ListSensors' Query. Allow users to pass in custom DataConnect instances. */
export function listSensors(options?: OperationOptions): Promise<ExecuteOperationResponse<ListSensorsData>>;

/** Generated Node Admin SDK operation action function for the 'ListSensorReadings' Query. Allow users to execute without passing in DataConnect. */
export function listSensorReadings(dc: DataConnect, options?: OperationOptions): Promise<ExecuteOperationResponse<ListSensorReadingsData>>;
/** Generated Node Admin SDK operation action function for the 'ListSensorReadings' Query. Allow users to pass in custom DataConnect instances. */
export function listSensorReadings(options?: OperationOptions): Promise<ExecuteOperationResponse<ListSensorReadingsData>>;

