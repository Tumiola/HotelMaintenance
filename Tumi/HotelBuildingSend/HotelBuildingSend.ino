#include <rn2xx3.h>

/*
 * Author: JP Meijers
 * 30 September 2016
 */

// ─── Device config — change this per device (1–50) ───────────────────────────
#define DEVICE_ID         1       // <<< CHANGE THIS per device
#define BASE_INTERVAL_MS  10000  // Base delay between sends (10 seconds)
#define JITTER_MS         8000   // Random jitter added on top (0–8 seconds)
// Each device will send every 10–18 seconds, randomly.
// With 50 devices this spreads traffic and minimises collisions.

#include <HardwareSerial.h>
#define TX1 D18;
#define RX1 D19;
#define LED_PIN 13
HardwareSerial loraSerial(1);

String str;

const int RST = 23;

void setup() {
  //output LED pin
  pinMode(13, OUTPUT);
  led_off();

  // Open serial communications and wait for port to open:

  pinMode(RST,OUTPUT);
  digitalWrite(RST,HIGH);

  Serial.begin(57600);

  loraSerial.begin(9600, SERIAL_8N1, 18, 19);
  loraSerial.setTimeout(60000);


  // hardware reset after UART init
  digitalWrite(RST, LOW);
  delay(200);
  digitalWrite(RST, HIGH);
  delay(200);

  lora_autobaud();

  led_on();
  delay(1000);
  led_off();

  Serial.println("Initing LoRa");

  loraSerial.read();
  str = loraSerial.readStringUntil('\n');

  Serial.println(str);
  loraSerial.println("sys get ver");

  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("mac pause");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  //  loraSerial.println("radio set bt 0.5");
  //  wait_for_ok();

  loraSerial.println("radio set mod lora");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set freq 869100000");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set pwr 14");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set sf sf7");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set afcbw 41.7");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set rxbw 125");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  //  loraSerial.println("radio set bitrate 50000");
  //  wait_for_ok();

  //  loraSerial.println("radio set fdev 25000");
  //  wait_for_ok();

  loraSerial.println("radio set prlen 8");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set crc on");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set iqi off");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set cr 4/5");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set wdt 60000");  //disable for continuous reception
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set sync 12");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  loraSerial.println("radio set bw 125");
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);

  randomSeed(esp_random());

  Serial.println("starting loop");
}
uint16_t counter = 0;

void loop() {
  led_on();
  String payload = buildPayload(counter);
  // Try sending this: 010102 01020929 01020947 from hex
  Serial.printf("[TX] Device:%02d Counter:%05d Payload:%s\n",
                DEVICE_ID, counter, payload.c_str());
  loraSerial.println("radio tx " + payload);
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);
  str = loraSerial.readStringUntil('\n');
  Serial.println(str);
  led_off();
  counter++;
  //delay(200);
  uint32_t waitMs = BASE_INTERVAL_MS + random(0, JITTER_MS);
  Serial.printf("Next TX in %lums\n", waitMs);
  delay(waitMs);
}

void lora_autobaud() {
  String response = "";
  while (response == "") {
    delay(1000);
    loraSerial.write((uint8_t)0x00);
    loraSerial.write((uint8_t)0x55);
    loraSerial.println();
    loraSerial.println("sys get ver");
    response = loraSerial.readStringUntil('\n');
  }
}

String buildPayload(uint16_t counter) {
  char buf[9];
  sprintf(buf, "%02X%02X%04X", DEVICE_ID, 0x01, counter);
  // 0x01 = MSG_TYPE heartbeat. We'll add more types when we do the handshake.
  return String(buf);
}

/*
 * This function blocks until the word "ok\n" is received on the UART,
 * or until a timeout of 3*5 seconds.
 */
int wait_for_ok() {
  str = loraSerial.readStringUntil('\n');
  if (str.indexOf("ok") == 0) {
    return 1;
  } else return 0;
}

void led_on() {
  digitalWrite(13, 1);
}

void led_off() {
  digitalWrite(13, 0);
}