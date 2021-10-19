#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

//http server code from https://randomnerdtutorials.com/esp32-web-server-arduino-ide/

const char* ssid = "MaxNet";
const char* password = "88888888";
#define MDNS_NAME "growbot-switcheroo"

WiFiServer server(80);
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;

#define NUM_PORTS 6
String header;
bool pendingSave = false;
bool portStatus[NUM_PORTS] = { false, false, false, false, false, false };
int relayPins[NUM_PORTS] = {18, 15, 19, 16, 21, 17};

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

void saveState() {
  byte b = portStatus2Byte();
  EEPROM.write(0, b);
  EEPROM.commit();
}

void loadState() {
  byte b = EEPROM.read(0);
  byte2portStatus(b);
  setPorts();
}
void setup() {
  Serial.begin(9600);
  EEPROM.begin(512);

  for (int i = 0; i < NUM_PORTS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  loadState();
  setPorts();
  WiFi.hostname(MDNS_NAME);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
   WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Setting up OTA...");
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.onStart([]() {
      Serial.println("Start updating");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("Update End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]\n: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.begin();

  Serial.println("Starting mDNS...");
  MDNS.begin(MDNS_NAME);

  Serial.println("Starting webserver...");
  server.begin();

  Serial.println("Done with startup");
}
void sendOk(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
}
void sendBad(WiFiClient& client) {
  Serial.println("bad request");
  client.println("HTTP/1.1 404 NOT FOUND");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
}

void loop() {

  ArduinoOTA.handle();

  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        //Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {

            if (header.indexOf("GET / ") >= 0) {
              sendOk(client);
              client.printf("%d", portStatus2Byte());
              Serial.println("Returned status to http");
            } else {
              int idx = header.indexOf("GET /");
              if (idx < 0) {
                sendBad(client);
              } else {
                bool bad = false;
                idx += 5;
                int idx2 = header.indexOf(" ", idx) ;
                int len = idx2 - idx;
                if (len > 0 && len <= 3) {
                  String num = header.substring(idx, idx2);
                  for (int i = 0; i < num.length(); i++) {
                    if (!isDigit(num[i])) {
                      bad = true;
                      break;
                    }
                  }
                  if (!bad) {
                    int portStatus = num.toInt();
                    if (portStatus >= 0 && portStatus <= 255) {
                      Serial.printf("Got new status %d from string %s\n", portStatus, num.c_str());
                        byte2portStatus((byte)portStatus);
                        sendOk(client);
                    } else {
                      Serial.printf("number was out of range %d\n", portStatus);  
                      sendBad(client);
                    }
                  } else {
                    Serial.printf("string wasn't number %s\n", num.c_str());  
                    sendBad(client);
                  }
                } else {
                  Serial.printf("length was bad %d\n", len);
                  sendBad(client);
                }                
              }
            }
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
  setPorts();
  if (pendingSave) {
    saveState();
    pendingSave = false;
  }
}