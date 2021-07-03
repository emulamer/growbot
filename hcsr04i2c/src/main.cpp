#include <Arduino.h>
#include <Wire.h>
#include <Ultrasonic.h>
#define I2C_ADDR 0x59
#define TRG_PIN 4
#define ECHO_PIN 5

float cmDist = NAN;
bool doRead = false;
int readtimes = 1;
Ultrasonic us(TRG_PIN, ECHO_PIN);
// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany)
{
  int x = Wire.read();
  if (x > 0 && x < 10) {
    cmDist = NAN;
    doRead = true;
    readtimes = x;
  }  
}
void requestEvent() {
  Wire.write((char *)&cmDist, 4);
}
void setup() {
  Serial.begin(9600);
  Wire.begin(I2C_ADDR);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

void loop() {
  doRead = true;
  if (doRead) {
    doRead = false;
    //Serial.println("got a 1, doing sample");
    //delay(100);
    int readCount = 0;
    float sum = 0;
    if (readtimes < 1 || readtimes > 10) {
      readtimes = 3;
    }
    for (int i = 0; i < readtimes; i++) {
      unsigned int reading = us.read();
      Serial.print("read ");
      Serial.println(reading);
      if (reading >= 200) {
        continue;
      } else {
        sum += reading;
        readCount++;
      }
    }
    if (readCount < 1) {
      cmDist = NAN;
    } else {
      cmDist = sum / readCount;
    }
    
    Serial.print("read ");
    Serial.println(cmDist);
  }
  delay(3000);
//  cmDist = NAN;
//     int readCount = 0;
//     float sum = 0;
//     for (int i = 0; i < 3; i++) {
//       unsigned int reading = us.read();
//       Serial.print("read ");
//       Serial.println(reading);
//       if (reading >= 1000) {
//         continue;
//       } else {
//         sum += reading;
//         readCount++;
//         break;
//       }
//     }
//     if (readCount < 1) {
//       cmDist = NAN;
//     } else {
//       cmDist = sum / readCount;
//     }
//     Serial.print("read ");
//     Serial.println(cmDist);
  // put your main code here, to run repeatedly:
}