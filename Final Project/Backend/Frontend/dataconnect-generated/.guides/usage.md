# Basic Usage

Always prioritize using a supported framework over using the generated SDK
directly. Supported frameworks simplify the developer experience and help ensure
best practices are followed.





## Advanced Usage
If a user is not using a supported framework, they can use the generated SDK directly.

Here's an example of how to use it with the first 5 operations:

```js
import { createSensor, createSensorReading, listSensors, listSensorReadings } from '@dataconnect/generated';


// Operation CreateSensor:  For variables, look at type CreateSensorVars in ../index.d.ts
const { data } = await CreateSensor(dataConnect, createSensorVars);

// Operation CreateSensorReading:  For variables, look at type CreateSensorReadingVars in ../index.d.ts
const { data } = await CreateSensorReading(dataConnect, createSensorReadingVars);

// Operation ListSensors: 
const { data } = await ListSensors(dataConnect);

// Operation ListSensorReadings: 
const { data } = await ListSensorReadings(dataConnect);


```