# Generated TypeScript README
This README will guide you through the process of using the generated JavaScript SDK package for the connector `example`. It will also provide examples on how to use your generated SDK to call your Data Connect queries and mutations.

***NOTE:** This README is generated alongside the generated SDK. If you make changes to this file, they will be overwritten when the SDK is regenerated.*

# Table of Contents
- [**Overview**](#generated-javascript-readme)
- [**Accessing the connector**](#accessing-the-connector)
  - [*Connecting to the local Emulator*](#connecting-to-the-local-emulator)
- [**Queries**](#queries)
  - [*ListSensors*](#listsensors)
  - [*ListSensorReadings*](#listsensorreadings)
- [**Mutations**](#mutations)
  - [*CreateSensor*](#createsensor)
  - [*CreateSensorReading*](#createsensorreading)

# Accessing the connector
A connector is a collection of Queries and Mutations. One SDK is generated for each connector - this SDK is generated for the connector `example`. You can find more information about connectors in the [Data Connect documentation](https://firebase.google.com/docs/data-connect#how-does).

You can use this generated SDK by importing from the package `@dataconnect/generated` as shown below. Both CommonJS and ESM imports are supported.

You can also follow the instructions from the [Data Connect documentation](https://firebase.google.com/docs/data-connect/web-sdk#set-client).

```typescript
import { getDataConnect } from 'firebase/data-connect';
import { connectorConfig } from '@dataconnect/generated';

const dataConnect = getDataConnect(connectorConfig);
```

## Connecting to the local Emulator
By default, the connector will connect to the production service.

To connect to the emulator, you can use the following code.
You can also follow the emulator instructions from the [Data Connect documentation](https://firebase.google.com/docs/data-connect/web-sdk#instrument-clients).

```typescript
import { connectDataConnectEmulator, getDataConnect } from 'firebase/data-connect';
import { connectorConfig } from '@dataconnect/generated';

const dataConnect = getDataConnect(connectorConfig);
connectDataConnectEmulator(dataConnect, 'localhost', 9399);
```

After it's initialized, you can call your Data Connect [queries](#queries) and [mutations](#mutations) from your generated SDK.

# Queries

There are two ways to execute a Data Connect Query using the generated Web SDK:
- Using a Query Reference function, which returns a `QueryRef`
  - The `QueryRef` can be used as an argument to `executeQuery()`, which will execute the Query and return a `QueryPromise`
- Using an action shortcut function, which returns a `QueryPromise`
  - Calling the action shortcut function will execute the Query and return a `QueryPromise`

The following is true for both the action shortcut function and the `QueryRef` function:
- The `QueryPromise` returned will resolve to the result of the Query once it has finished executing
- If the Query accepts arguments, both the action shortcut function and the `QueryRef` function accept a single argument: an object that contains all the required variables (and the optional variables) for the Query
- Both functions can be called with or without passing in a `DataConnect` instance as an argument. If no `DataConnect` argument is passed in, then the generated SDK will call `getDataConnect(connectorConfig)` behind the scenes for you.

Below are examples of how to use the `example` connector's generated functions to execute each query. You can also follow the examples from the [Data Connect documentation](https://firebase.google.com/docs/data-connect/web-sdk#using-queries).

## ListSensors
You can execute the `ListSensors` query using the following action shortcut function, or by calling `executeQuery()` after calling the following `QueryRef` function, both of which are defined in [dataconnect-generated/index.d.ts](./index.d.ts):
```typescript
listSensors(options?: ExecuteQueryOptions): QueryPromise<ListSensorsData, undefined>;

interface ListSensorsRef {
  ...
  /* Allow users to create refs without passing in DataConnect */
  (): QueryRef<ListSensorsData, undefined>;
}
export const listSensorsRef: ListSensorsRef;
```
You can also pass in a `DataConnect` instance to the action shortcut function or `QueryRef` function.
```typescript
listSensors(dc: DataConnect, options?: ExecuteQueryOptions): QueryPromise<ListSensorsData, undefined>;

interface ListSensorsRef {
  ...
  (dc: DataConnect): QueryRef<ListSensorsData, undefined>;
}
export const listSensorsRef: ListSensorsRef;
```

If you need the name of the operation without creating a ref, you can retrieve the operation name by calling the `operationName` property on the listSensorsRef:
```typescript
const name = listSensorsRef.operationName;
console.log(name);
```

### Variables
The `ListSensors` query has no variables.
### Return Type
Recall that executing the `ListSensors` query returns a `QueryPromise` that resolves to an object with a `data` property.

The `data` property is an object of type `ListSensorsData`, which is defined in [dataconnect-generated/index.d.ts](./index.d.ts). It has the following fields:
```typescript
export interface ListSensorsData {
  sensors: ({
    id: UUIDString;
    name: string;
    type: SensorType;
    unit: string;
  } & Sensor_Key)[];
}
```
### Using `ListSensors`'s action shortcut function

```typescript
import { getDataConnect } from 'firebase/data-connect';
import { connectorConfig, listSensors } from '@dataconnect/generated';


// Call the `listSensors()` function to execute the query.
// You can use the `await` keyword to wait for the promise to resolve.
const { data } = await listSensors();

// You can also pass in a `DataConnect` instance to the action shortcut function.
const dataConnect = getDataConnect(connectorConfig);
const { data } = await listSensors(dataConnect);

console.log(data.sensors);

// Or, you can use the `Promise` API.
listSensors().then((response) => {
  const data = response.data;
  console.log(data.sensors);
});
```

### Using `ListSensors`'s `QueryRef` function

```typescript
import { getDataConnect, executeQuery } from 'firebase/data-connect';
import { connectorConfig, listSensorsRef } from '@dataconnect/generated';


// Call the `listSensorsRef()` function to get a reference to the query.
const ref = listSensorsRef();

// You can also pass in a `DataConnect` instance to the `QueryRef` function.
const dataConnect = getDataConnect(connectorConfig);
const ref = listSensorsRef(dataConnect);

// Call `executeQuery()` on the reference to execute the query.
// You can use the `await` keyword to wait for the promise to resolve.
const { data } = await executeQuery(ref);

console.log(data.sensors);

// Or, you can use the `Promise` API.
executeQuery(ref).then((response) => {
  const data = response.data;
  console.log(data.sensors);
});
```

## ListSensorReadings
You can execute the `ListSensorReadings` query using the following action shortcut function, or by calling `executeQuery()` after calling the following `QueryRef` function, both of which are defined in [dataconnect-generated/index.d.ts](./index.d.ts):
```typescript
listSensorReadings(options?: ExecuteQueryOptions): QueryPromise<ListSensorReadingsData, undefined>;

interface ListSensorReadingsRef {
  ...
  /* Allow users to create refs without passing in DataConnect */
  (): QueryRef<ListSensorReadingsData, undefined>;
}
export const listSensorReadingsRef: ListSensorReadingsRef;
```
You can also pass in a `DataConnect` instance to the action shortcut function or `QueryRef` function.
```typescript
listSensorReadings(dc: DataConnect, options?: ExecuteQueryOptions): QueryPromise<ListSensorReadingsData, undefined>;

interface ListSensorReadingsRef {
  ...
  (dc: DataConnect): QueryRef<ListSensorReadingsData, undefined>;
}
export const listSensorReadingsRef: ListSensorReadingsRef;
```

If you need the name of the operation without creating a ref, you can retrieve the operation name by calling the `operationName` property on the listSensorReadingsRef:
```typescript
const name = listSensorReadingsRef.operationName;
console.log(name);
```

### Variables
The `ListSensorReadings` query has no variables.
### Return Type
Recall that executing the `ListSensorReadings` query returns a `QueryPromise` that resolves to an object with a `data` property.

The `data` property is an object of type `ListSensorReadingsData`, which is defined in [dataconnect-generated/index.d.ts](./index.d.ts). It has the following fields:
```typescript
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
```
### Using `ListSensorReadings`'s action shortcut function

```typescript
import { getDataConnect } from 'firebase/data-connect';
import { connectorConfig, listSensorReadings } from '@dataconnect/generated';


// Call the `listSensorReadings()` function to execute the query.
// You can use the `await` keyword to wait for the promise to resolve.
const { data } = await listSensorReadings();

// You can also pass in a `DataConnect` instance to the action shortcut function.
const dataConnect = getDataConnect(connectorConfig);
const { data } = await listSensorReadings(dataConnect);

console.log(data.sensorReadings);

// Or, you can use the `Promise` API.
listSensorReadings().then((response) => {
  const data = response.data;
  console.log(data.sensorReadings);
});
```

### Using `ListSensorReadings`'s `QueryRef` function

```typescript
import { getDataConnect, executeQuery } from 'firebase/data-connect';
import { connectorConfig, listSensorReadingsRef } from '@dataconnect/generated';


// Call the `listSensorReadingsRef()` function to get a reference to the query.
const ref = listSensorReadingsRef();

// You can also pass in a `DataConnect` instance to the `QueryRef` function.
const dataConnect = getDataConnect(connectorConfig);
const ref = listSensorReadingsRef(dataConnect);

// Call `executeQuery()` on the reference to execute the query.
// You can use the `await` keyword to wait for the promise to resolve.
const { data } = await executeQuery(ref);

console.log(data.sensorReadings);

// Or, you can use the `Promise` API.
executeQuery(ref).then((response) => {
  const data = response.data;
  console.log(data.sensorReadings);
});
```

# Mutations

There are two ways to execute a Data Connect Mutation using the generated Web SDK:
- Using a Mutation Reference function, which returns a `MutationRef`
  - The `MutationRef` can be used as an argument to `executeMutation()`, which will execute the Mutation and return a `MutationPromise`
- Using an action shortcut function, which returns a `MutationPromise`
  - Calling the action shortcut function will execute the Mutation and return a `MutationPromise`

The following is true for both the action shortcut function and the `MutationRef` function:
- The `MutationPromise` returned will resolve to the result of the Mutation once it has finished executing
- If the Mutation accepts arguments, both the action shortcut function and the `MutationRef` function accept a single argument: an object that contains all the required variables (and the optional variables) for the Mutation
- Both functions can be called with or without passing in a `DataConnect` instance as an argument. If no `DataConnect` argument is passed in, then the generated SDK will call `getDataConnect(connectorConfig)` behind the scenes for you.

Below are examples of how to use the `example` connector's generated functions to execute each mutation. You can also follow the examples from the [Data Connect documentation](https://firebase.google.com/docs/data-connect/web-sdk#using-mutations).

## CreateSensor
You can execute the `CreateSensor` mutation using the following action shortcut function, or by calling `executeMutation()` after calling the following `MutationRef` function, both of which are defined in [dataconnect-generated/index.d.ts](./index.d.ts):
```typescript
createSensor(vars: CreateSensorVariables): MutationPromise<CreateSensorData, CreateSensorVariables>;

interface CreateSensorRef {
  ...
  /* Allow users to create refs without passing in DataConnect */
  (vars: CreateSensorVariables): MutationRef<CreateSensorData, CreateSensorVariables>;
}
export const createSensorRef: CreateSensorRef;
```
You can also pass in a `DataConnect` instance to the action shortcut function or `MutationRef` function.
```typescript
createSensor(dc: DataConnect, vars: CreateSensorVariables): MutationPromise<CreateSensorData, CreateSensorVariables>;

interface CreateSensorRef {
  ...
  (dc: DataConnect, vars: CreateSensorVariables): MutationRef<CreateSensorData, CreateSensorVariables>;
}
export const createSensorRef: CreateSensorRef;
```

If you need the name of the operation without creating a ref, you can retrieve the operation name by calling the `operationName` property on the createSensorRef:
```typescript
const name = createSensorRef.operationName;
console.log(name);
```

### Variables
The `CreateSensor` mutation requires an argument of type `CreateSensorVariables`, which is defined in [dataconnect-generated/index.d.ts](./index.d.ts). It has the following fields:

```typescript
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
```
### Return Type
Recall that executing the `CreateSensor` mutation returns a `MutationPromise` that resolves to an object with a `data` property.

The `data` property is an object of type `CreateSensorData`, which is defined in [dataconnect-generated/index.d.ts](./index.d.ts). It has the following fields:
```typescript
export interface CreateSensorData {
  sensor_insert: Sensor_Key;
}
```
### Using `CreateSensor`'s action shortcut function

```typescript
import { getDataConnect } from 'firebase/data-connect';
import { connectorConfig, createSensor, CreateSensorVariables } from '@dataconnect/generated';

// The `CreateSensor` mutation requires an argument of type `CreateSensorVariables`:
const createSensorVars: CreateSensorVariables = {
  sensorCollectionId: ..., 
  name: ..., 
  type: ..., 
  unit: ..., 
  description: ..., // optional
  serialNumber: ..., // optional
  locationDetails: ..., // optional
  createdAt: ..., 
};

// Call the `createSensor()` function to execute the mutation.
// You can use the `await` keyword to wait for the promise to resolve.
const { data } = await createSensor(createSensorVars);
// Variables can be defined inline as well.
const { data } = await createSensor({ sensorCollectionId: ..., name: ..., type: ..., unit: ..., description: ..., serialNumber: ..., locationDetails: ..., createdAt: ..., });

// You can also pass in a `DataConnect` instance to the action shortcut function.
const dataConnect = getDataConnect(connectorConfig);
const { data } = await createSensor(dataConnect, createSensorVars);

console.log(data.sensor_insert);

// Or, you can use the `Promise` API.
createSensor(createSensorVars).then((response) => {
  const data = response.data;
  console.log(data.sensor_insert);
});
```

### Using `CreateSensor`'s `MutationRef` function

```typescript
import { getDataConnect, executeMutation } from 'firebase/data-connect';
import { connectorConfig, createSensorRef, CreateSensorVariables } from '@dataconnect/generated';

// The `CreateSensor` mutation requires an argument of type `CreateSensorVariables`:
const createSensorVars: CreateSensorVariables = {
  sensorCollectionId: ..., 
  name: ..., 
  type: ..., 
  unit: ..., 
  description: ..., // optional
  serialNumber: ..., // optional
  locationDetails: ..., // optional
  createdAt: ..., 
};

// Call the `createSensorRef()` function to get a reference to the mutation.
const ref = createSensorRef(createSensorVars);
// Variables can be defined inline as well.
const ref = createSensorRef({ sensorCollectionId: ..., name: ..., type: ..., unit: ..., description: ..., serialNumber: ..., locationDetails: ..., createdAt: ..., });

// You can also pass in a `DataConnect` instance to the `MutationRef` function.
const dataConnect = getDataConnect(connectorConfig);
const ref = createSensorRef(dataConnect, createSensorVars);

// Call `executeMutation()` on the reference to execute the mutation.
// You can use the `await` keyword to wait for the promise to resolve.
const { data } = await executeMutation(ref);

console.log(data.sensor_insert);

// Or, you can use the `Promise` API.
executeMutation(ref).then((response) => {
  const data = response.data;
  console.log(data.sensor_insert);
});
```

## CreateSensorReading
You can execute the `CreateSensorReading` mutation using the following action shortcut function, or by calling `executeMutation()` after calling the following `MutationRef` function, both of which are defined in [dataconnect-generated/index.d.ts](./index.d.ts):
```typescript
createSensorReading(vars: CreateSensorReadingVariables): MutationPromise<CreateSensorReadingData, CreateSensorReadingVariables>;

interface CreateSensorReadingRef {
  ...
  /* Allow users to create refs without passing in DataConnect */
  (vars: CreateSensorReadingVariables): MutationRef<CreateSensorReadingData, CreateSensorReadingVariables>;
}
export const createSensorReadingRef: CreateSensorReadingRef;
```
You can also pass in a `DataConnect` instance to the action shortcut function or `MutationRef` function.
```typescript
createSensorReading(dc: DataConnect, vars: CreateSensorReadingVariables): MutationPromise<CreateSensorReadingData, CreateSensorReadingVariables>;

interface CreateSensorReadingRef {
  ...
  (dc: DataConnect, vars: CreateSensorReadingVariables): MutationRef<CreateSensorReadingData, CreateSensorReadingVariables>;
}
export const createSensorReadingRef: CreateSensorReadingRef;
```

If you need the name of the operation without creating a ref, you can retrieve the operation name by calling the `operationName` property on the createSensorReadingRef:
```typescript
const name = createSensorReadingRef.operationName;
console.log(name);
```

### Variables
The `CreateSensorReading` mutation requires an argument of type `CreateSensorReadingVariables`, which is defined in [dataconnect-generated/index.d.ts](./index.d.ts). It has the following fields:

```typescript
export interface CreateSensorReadingVariables {
  sensorId: UUIDString;
  value: number;
  unit?: string | null;
  description?: string | null;
  timestamp: TimestampString;
}
```
### Return Type
Recall that executing the `CreateSensorReading` mutation returns a `MutationPromise` that resolves to an object with a `data` property.

The `data` property is an object of type `CreateSensorReadingData`, which is defined in [dataconnect-generated/index.d.ts](./index.d.ts). It has the following fields:
```typescript
export interface CreateSensorReadingData {
  sensorReading_insert: SensorReading_Key;
}
```
### Using `CreateSensorReading`'s action shortcut function

```typescript
import { getDataConnect } from 'firebase/data-connect';
import { connectorConfig, createSensorReading, CreateSensorReadingVariables } from '@dataconnect/generated';

// The `CreateSensorReading` mutation requires an argument of type `CreateSensorReadingVariables`:
const createSensorReadingVars: CreateSensorReadingVariables = {
  sensorId: ..., 
  value: ..., 
  unit: ..., // optional
  description: ..., // optional
  timestamp: ..., 
};

// Call the `createSensorReading()` function to execute the mutation.
// You can use the `await` keyword to wait for the promise to resolve.
const { data } = await createSensorReading(createSensorReadingVars);
// Variables can be defined inline as well.
const { data } = await createSensorReading({ sensorId: ..., value: ..., unit: ..., description: ..., timestamp: ..., });

// You can also pass in a `DataConnect` instance to the action shortcut function.
const dataConnect = getDataConnect(connectorConfig);
const { data } = await createSensorReading(dataConnect, createSensorReadingVars);

console.log(data.sensorReading_insert);

// Or, you can use the `Promise` API.
createSensorReading(createSensorReadingVars).then((response) => {
  const data = response.data;
  console.log(data.sensorReading_insert);
});
```

### Using `CreateSensorReading`'s `MutationRef` function

```typescript
import { getDataConnect, executeMutation } from 'firebase/data-connect';
import { connectorConfig, createSensorReadingRef, CreateSensorReadingVariables } from '@dataconnect/generated';

// The `CreateSensorReading` mutation requires an argument of type `CreateSensorReadingVariables`:
const createSensorReadingVars: CreateSensorReadingVariables = {
  sensorId: ..., 
  value: ..., 
  unit: ..., // optional
  description: ..., // optional
  timestamp: ..., 
};

// Call the `createSensorReadingRef()` function to get a reference to the mutation.
const ref = createSensorReadingRef(createSensorReadingVars);
// Variables can be defined inline as well.
const ref = createSensorReadingRef({ sensorId: ..., value: ..., unit: ..., description: ..., timestamp: ..., });

// You can also pass in a `DataConnect` instance to the `MutationRef` function.
const dataConnect = getDataConnect(connectorConfig);
const ref = createSensorReadingRef(dataConnect, createSensorReadingVars);

// Call `executeMutation()` on the reference to execute the mutation.
// You can use the `await` keyword to wait for the promise to resolve.
const { data } = await executeMutation(ref);

console.log(data.sensorReading_insert);

// Or, you can use the `Promise` API.
executeMutation(ref).then((response) => {
  const data = response.data;
  console.log(data.sensorReading_insert);
});
```

