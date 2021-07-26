#include <Arduino.h>
#include <Wire.h>
#define I2C_ADDR 0x59
#define ANALOG_V_PIN 0
#define R1_OHMS 668

#define FLOAT_MIN_OHMS 35.0
#define FLOAT_MAX_OHMS 240.0

#define SAMPLE_INTERVAL 250

unsigned long lastReadStamp = 0;

int rawAnalogRead = -1;

void requestEvent() {
  //TODO: this thing's only 2 bytes, fix it coordinated with changes to the esp code
  Wire.write((char *)&rawAnalogRead, 4);
}
void setup() {
  Serial.begin(9600);
  Wire.begin(I2C_ADDR);
  Wire.onRequest(requestEvent);
}


void loop()
{
  if (millis() - lastReadStamp > SAMPLE_INTERVAL) {
    rawAnalogRead = analogRead(ANALOG_V_PIN);
    Serial.print("Raw analog: ");
    Serial.println(rawAnalogRead);
    lastReadStamp = millis();
  }

}

