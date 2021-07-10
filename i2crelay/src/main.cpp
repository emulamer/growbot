#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#define I2C_ADDR 0x28

#define NUM_PORTS 6

bool pendingSave = false;
bool portStatus[NUM_PORTS] = { false, false, false, false, false, false };
int relayPins[NUM_PORTS] = {7, 8, 9, 10, 11, 12};

int digitalReadOutputPin(uint8_t pin)
{
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  if (port == NOT_A_PIN) 
    return LOW;

  return (*portOutputRegister(port) & bit) ? HIGH : LOW;
}

void setPorts() {
  for (int i = 0; i < NUM_PORTS; i++) {
    digitalWrite(relayPins[i], portStatus[i]?HIGH:LOW);
    // Serial.print("Set port ");
    // Serial.print(i);
    // Serial.print(" (pin ");
    // Serial.print(relayPins[i]);
    // Serial.print(") to ");
    // Serial.println(portStatus[i]?"ON":"OFF");
  }
}

bool byte2portStatus(byte b) {
  bool change = false;
  for (int i = 0; i < NUM_PORTS; i++) {
    bool on = (((1<<i) & b) != 0);
    if (portStatus[i] != on) {
      change = true;
    }    
    portStatus[i] = on;
  }
  return change;
}

byte portStatus2Byte() {
  byte b = 0;
  for (int i = 0; i < NUM_PORTS; i++) {
    b = b | (1<<i);
  }
  return b;
}

void receiveEvent(int howMany)
{
  int x = Wire.read();
  if (x < 0) { return; }
  byte b = (byte)x;
  if (byte2portStatus(b)) {
    pendingSave = true;
  }
}
void requestEvent() {
  Wire.write(portStatus2Byte());
}
void saveState() {
  byte b = portStatus2Byte();
  EEPROM.update(0, b);
}

void loadState() {
  byte b = EEPROM.read(0);
  byte2portStatus(b);
  setPorts();
}
void setup() {
  Serial.begin(9600);
  EEPROM.begin();
  for (int i = 0; i < NUM_PORTS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  loadState();
  setPorts();
  Wire.begin(I2C_ADDR);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

void loop() {
  delay(100);
  // int r = Serial.read();
  // if (r > 0) {
  //   portStatus[0] = !portStatus[0];
  // }
  setPorts();
  if (pendingSave) {
    saveState();
    pendingSave = false;
  }
}