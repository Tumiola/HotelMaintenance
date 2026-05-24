import { ConnectorConfig, DataConnect, QueryRef, QueryPromise, ExecuteQueryOptions, MutationRef, MutationPromise, DataConnectSettings } from 'firebase/data-connect';

export const connectorConfig: ConnectorConfig;
export const dataConnectSettings: DataConnectSettings;

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
};



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

interface CreateSensorRef {
  /* Allow users to create refs without passing in DataConnect */
  (vars: CreateSensorVariables): MutationRef<CreateSensorData, CreateSensorVariables>;
  /* Allow users to pass in custom DataConnect instances */
  (dc: DataConnect, vars: CreateSensorVariables): MutationRef<CreateSensorData, CreateSensorVariables>;
  operationName: string;
}
export const createSensorRef: CreateSensorRef;

export function createSensor(vars: CreateSensorVariables): MutationPromise<CreateSensorData, CreateSensorVariables>;
export function createSensor(dc: DataConnect, vars: CreateSensorVariables): MutationPromise<CreateSensorData, CreateSensorVariables>;

interface CreateSensorReadingRef {
  /* Allow users to create refs without passing in DataConnect */
  (vars: CreateSensorReadingVariables): MutationRef<CreateSensorReadingData, CreateSensorReadingVariables>;
  /* Allow users to pass in custom DataConnect instances */
  (dc: DataConnect, vars: CreateSensorReadingVariables): MutationRef<CreateSensorReadingData, CreateSensorReadingVariables>;
  operationName: string;
}
export const createSensorReadingRef: CreateSensorReadingRef;

export function createSensorReading(vars: CreateSensorReadingVariables): MutationPromise<CreateSensorReadingData, CreateSensorReadingVariables>;
export function createSensorReading(dc: DataConnect, vars: CreateSensorReadingVariables): MutationPromise<CreateSensorReadingData, CreateSensorReadingVariables>;

interface ListSensorsRef {
  /* Allow users to create refs without passing in DataConnect */
  (): QueryRef<ListSensorsData, undefined>;
  /* Allow users to pass in custom DataConnect instances */
  (dc: DataConnect): QueryRef<ListSensorsData, undefined>;
  operationName: string;
}
export const listSensorsRef: ListSensorsRef;

export function listSensors(options?: ExecuteQueryOptions): QueryPromise<ListSensorsData, undefined>;
export function listSensors(dc: DataConnect, options?: ExecuteQueryOptions): QueryPromise<ListSensorsData, undefined>;

interface ListSensorReadingsRef {
  /* Allow users to create refs without passing in DataConnect */
  (): QueryRef<ListSensorReadingsData, undefined>;
  /* Allow users to pass in custom DataConnect instances */
  (dc: DataConnect): QueryRef<ListSensorReadingsData, undefined>;
  operationName: string;
}
export const listSensorReadingsRef: ListSensorReadingsRef;

export function listSensorReadings(options?: ExecuteQueryOptions): QueryPromise<ListSensorReadingsData, undefined>;
export function listSensorReadings(dc: DataConnect, options?: ExecuteQueryOptions): QueryPromise<ListSensorReadingsData, undefined>;

