#include <Arduino.h>


#define CLOCK_PIN 10 //CH = SHCP (clock)
#define LATCH_PIN 11 //ST = STCP  (latch)
#define DATA_PIN 12 //DA = DS  data

#define PORT_OFFSET 2

float mlPerSecPort1;
float mlPerSecPort2;
float mlPerSecPort3;
float mlPerSecPort4;
float mlPerSecPort5;
float mlPerSecPort6;


byte portFlags = 0;


void updatePorts() {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, portFlags);
  digitalWrite(LATCH_PIN, HIGH);
}

void setPort(byte portNum, bool on) {
  if (on) {
    portFlags |= (1<<(7 - portNum));
  } else {
    portFlags &= ~(1<<(7 - portNum));
  }
  updatePorts();
}

void allOff() {
  portFlags = 0;
  updatePorts();
}


//CH = SHCP (clock)
//DA = DS  data
//ST = STCP  (latch)
void setup() {
  Serial.begin(9600);
  // put your setup code here, to run once:
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
}

void loop() {
  Serial.println("starting loop");
  allOff();
  setPort(0, true);
  Serial.println("0 on");
  delay(1000);
  allOff();
  setPort(1, true);
  Serial.println("1 on");
  delay(1000);
  allOff();
  setPort(2, true);
  Serial.println("2 on");
  delay(1000);
  allOff();
  setPort(3, true);
  Serial.println("3 on");
  delay(1000);
  allOff();
  setPort(4, true);
  Serial.println("4 on");
  delay(1000);
  allOff();
  setPort(5, true);
  Serial.println("5 on");
  delay(1000);
  // put your main code here, to run repeatedly:
}