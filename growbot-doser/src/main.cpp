#define LOG_UDP_PORT 44445
#include <GrowbotCommon.h>
#include <Wifi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include <Ticker.h>
#include "DebugUtils.h"
#include <EEPROM.h>

#define WIFI_SSID "MaxNet"
#define WIFI_PASSWORD "88888888"
#define MDNS_NAME "growbot-doser"
#define WEBSOCKET_PORT 8118

#define MAX_DISPENSE_ML_AMOUNT 100
#define MAX_DISPENSE_MS 10000
#define MAX_CALIBRATION_VAL 10

/* stops dispensing everything
  format:
    (nothing)
*/
#define MSG_STOP_ALL 0x90

/* starts dispensing a specific number of milliliters on a port
  format:
    [0]: port number 0-5
    [1-4]: float value 
*/
#define MSG_DISPENSE_ML 0xA0

/* starts dispensing for a specific amount of time on a port
  format:
    [0]: port number 0-5
    [1-4]: int32 value 
*/
#define MSG_DISPENSE_MILLISEC 0xA1

/* requests the milliliter per second calibration value for a port
  format: 
    [0]: port number 0-5
*/
#define MSG_CALIBRATION_GET 0xB1

/* sets the milliliter per second calibration value for a port
  format: 
    [0]: port number 0-5
    [1-4]: float value of how many milliliters flow per second
*/
#define MSG_CALIBRATION_SET 0xB2

/* dispensing has started
  format:
      [0]: port number
      [1-4]: how many ML is being dispensed
*/
#define RSP_DISPENSE_START 0x01

/* dispensing has stopped
  format:
      [0]: port number
*/
#define RSP_DISPENSE_END 0x02

/* sent as a response to calibration_set or calibration_get
  format:
      [0]: port number
      [1-4]: float value of how many milliliters flow per second
*/
#define RSP_CALIBRATION_VALUE 0x03

/* send if the command fails
  format:
    (none)
*/
#define RSP_NACK 0xFF

#define NUM_PORTS 6

#define CLOCK_PIN 13 //CH = SHCP (clock)
#define LATCH_PIN 12 //ST = STCP  (latch)
#define DATA_PIN 14 //DA = DS  data

#define PORT_OFFSET 2

struct PortTimer {
  bool running;
  unsigned long startStamp;
  int millisecs;
};

float portCalibrations[NUM_PORTS];

#define WIFI_RECONNECT_DELAY 15000
unsigned long reconnectWifiAt = 0;

PortTimer portTimers[NUM_PORTS] = {
  {false, 0, 0}, {false, 0, 0}, {false, 0, 0}, {false, 0, 0}, {false, 0, 0}, {false, 0, 0}
};

WebSocketsServer webSocket(WEBSOCKET_PORT);    
byte portFlags = 0;

void loadCalibrations() {
  byte* bref = (byte*)portCalibrations;
  for (int i = 0; i < NUM_PORTS * 4; i++) {
    *(bref + i) = EEPROM.read(i);
  }
}

void saveCalibrations() {
  byte* bref = (byte*)portCalibrations;
  for (int i = 0; i < NUM_PORTS * 4; i++) {
    EEPROM.write(i, *(bref + i));
  }
  EEPROM.commit();
}

void updatePorts() {
  dbg.dprintf("Updating ports, 0: %s, 1: %s, 2: %s, 3: %s, 4: %s, 5: %s\n",
      ((portFlags & (1<<(PORT_OFFSET + 5))) > 1)?"ON":"off",
      ((portFlags & (1<<(PORT_OFFSET + 4))) > 1)?"ON":"off",
      ((portFlags & (1<<(PORT_OFFSET + 3))) > 1)?"ON":"off",
      ((portFlags & (1<<(PORT_OFFSET + 2))) > 1)?"ON":"off",
      ((portFlags & (1<<(PORT_OFFSET + 1))) > 1)?"ON":"off",
       ((portFlags & (1<<PORT_OFFSET)) > 1)?"ON":"off");
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

void stopAll() {
  allOff();
  //todo: cancel any tickers
}
void endDispense(byte port) {
  setPort(port, false);
  byte msg[2] = {
    RSP_DISPENSE_END,
    port
  };
  webSocket.broadcastBIN(msg, 2);
}
void startDispenseForMS(byte port, int millisec) {
  if (portTimers[port].running) {
    dbg.wprintf("Warining: dispense in progress but starting another dispense on port %d.\n", port);
  }
  dbg.printf("Starting dispense on port %d for %d millisec\n", port, millisec);
  portTimers[port].startStamp = millis();
  portTimers[port].millisecs = millisec;
  portTimers[port].running = true;
  float dispenseAmount = millisec * portCalibrations[port] / 1000;
  byte msg[6] = {
    RSP_DISPENSE_START,
    port
  };
  msg[0] = RSP_DISPENSE_START;
  msg[1] = port;
  memcpy(msg + 2, &dispenseAmount, 4);
  setPort(port, true);
  webSocket.broadcastBIN(msg, 6);
}



void dispenseML(byte port, float ml) {
  int ms = (int)(ml/portCalibrations[port]*1000);
  dbg.printf("Port %d dispensing %f ml for %d ms based on %f calibration\n", port, ml, ms, portCalibrations[port]);
  startDispenseForMS(port, ms);
}


void portTimerElapsed(byte port) {
  endDispense(port);
  portTimers[port].running = false;
  portTimers[port].startStamp = 0;
  portTimers[port].millisecs = 0;  
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) { // When a WebSocket message is received
  byte utility[6] = { 0, 0, 0, 0, 0, 0 };
  switch (type) {
    case WStype_DISCONNECTED:
      dbg.println("websocket disconnected");
      break;
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        dbg.println("websocket connected");
      }
      break;
    case WStype_TEXT:
      dbg.dprintln("websocket text");
      dbg.dprintf("payload: %s", payload);
      break;
    case WStype_BIN:
        dbg.dprintln("websocket binary");
        switch (payload[0]) {
          default:
            dbg.eprintf("unknown command %d\n", payload[0]);
            break;
          case MSG_STOP_ALL:
            //todo: figure out if anything is dispensing and send end messages?
            stopAll();
          break;
          case MSG_DISPENSE_ML:
            if (length < 6) {
              //bad message
              dbg.eprintf("Bad message length %d for dispense ml msg, needed 6\n", length);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            }
            if (payload[1] > 5) {
              dbg.eprintf("Bad port number %d for dispense ml msg, should be 0-5\n", payload[1]);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            } 
            if (*((float*)(payload + 2)) <= 0 || *((float*)(payload + 2)) > MAX_DISPENSE_ML_AMOUNT) {
              dbg.eprintf("Bad ML value %f for dispense ml msg, should be > 0 and <= %d\n", *((float*)(payload + 2)), MAX_DISPENSE_ML_AMOUNT);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            }          
            dispenseML(payload[1], *((float*)(payload + 2)));
          break;
          case MSG_DISPENSE_MILLISEC:
            if (length < 6) {
                //bad message
                dbg.eprintf("Bad message length %d for dispense millisec msg, needed 6\n", length);
                utility[0] = 0xFF;
                webSocket.sendBIN(num, utility, 1);
                return;
            }
            if (payload[1] > 5) {
              dbg.eprintf("Bad port number %d for dispense millisec msg, should be 0-5\n", payload[1]);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            } 
            if (*((int*)(payload + 2)) <= 0 || *((int*)(payload + 2)) > MAX_DISPENSE_MS) {
              dbg.eprintf("Bad MS value %d for dispense millisec msg, should be > 0 and <= %d\n", *((int*)(payload + 2)), MAX_DISPENSE_MS);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            }
            startDispenseForMS(payload[1], *((int*)(payload + 2)));              
          break;
          case MSG_CALIBRATION_SET:
            if (length < 6) {
              //bad message
              dbg.eprintf("Bad message length %d for set calibration msg, needed 6\n", length);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            }
            dbg.println("length ok");
            if (payload[1] > 5) {
              dbg.eprintf("Bad port number %d for set calibration msg, should be 0-5\n", payload[1]);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            } 
            dbg.println("port ok");
            float val;
            memcpy(&val, &payload[2], 4);
            if (val <= 0 || val > MAX_CALIBRATION_VAL) {
              dbg.eprintf("Bad ML/sec value %f for set calibration msg, should be > 0 and <= %d\n", val, MAX_CALIBRATION_VAL);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            }
            dbg.dprintln("float ok");
            portCalibrations[payload[1]] = val;
            dbg.dprintln("set in mem ok");
            saveCalibrations();
            dbg.dprintln("wrote eeprom ok");
            utility[0] = RSP_CALIBRATION_VALUE;
            utility[1] = payload[1];
            memcpy(utility+2, &portCalibrations[payload[1]], 4);
            webSocket.broadcastBIN(utility, 6);
          break;
          case MSG_CALIBRATION_GET:
              if (length < 2) {
              //bad message
              dbg.eprintf("Bad message length %d for get calibration msg, needed 2\n", length);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            }
            if (payload[1] > 5) {
              dbg.eprintf("Bad port number %d for get calibration msg, should be 0-5\n", payload[1]);
              utility[0] = 0xFF;
              webSocket.sendBIN(num, utility, 1);
              return;
            }
            utility[0] = RSP_CALIBRATION_VALUE;
            utility[1] = payload[1];
            memcpy(utility+2, &portCalibrations[payload[1]], 4);
            webSocket.broadcastBIN(utility, 6);
          break;
        }
      break;
  }
}
void setup() {
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  allOff();
  Serial.begin(9600);
  growbotCommonSetup(MDNS_NAME, WIFI_SSID, WIFI_PASSWORD, []() {
    dbg.println("Update starting, stopping all dosing");
    stopAll();
  });
  dbg.println("growbot-doser starting...");
  dbg.println("Initting EEPROM...");
  EEPROM.begin(512);
  dbg.println("Loading calibrations...");
  loadCalibrations();
    
  dbg.println("Starting websocket...");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  dbg.println("Done with startup");
}


void loop() {
  webSocket.loop();
  growbotCommonLoop();
  for (int i = 0; i < NUM_PORTS; i++) {
    if (!portTimers[i].running) {
      continue;
    }
    if (millis() - portTimers[i].startStamp > portTimers[i].millisecs) {
      portTimerElapsed(i);
    }
  }
}

