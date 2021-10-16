#include <Arduino.h>
#include <Wire.h>
#define I2C_ADDR 0x59
#define ANALOG_V_PIN A6


#define SAMPLE_INTERVAL 1000

unsigned long lastReadStamp = 0;

int rawAnalogRead = -1;

void requestEvent() {
  uint32_t read = analogRead(ANALOG_V_PIN);
  Serial.print("Raw read: ");
  Serial.println(read);
  Wire.write((char *)&read, 4);
}
void setup() {
  Serial.begin(9600);
  Wire.begin(I2C_ADDR);
  Wire.onRequest(requestEvent);
}


void loop()
{
   if (millis() - lastReadStamp > SAMPLE_INTERVAL) {
    uint32_t read = analogRead(ANALOG_V_PIN);
    Serial.print("Raw read: ");
    Serial.println(read);
    lastReadStamp = millis();
  }

}

