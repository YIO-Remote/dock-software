#include <Arduino.h>
#include <WiFi.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"
#include <Preferences.h>
#include <ESPmDNS.h>

// OTA service
#include <service_ota.h>
OTA ota;

// IR service
#include <service_ir.h>
InfraredService irservice;

// websocket server
#include <WebSocketsServer.h>
WebSocketsServer webSocketServer = WebSocketsServer(946);
StaticJsonDocument<200> wsdoc;
String token = "0";

uint8_t serverClients[100] = {};
int serverClientCount = 0;

StaticJsonDocument<200> doc;
// bool needsSetup = true;

////////////////////////////////////////////////////////////////
// STATE MACHINE
////////////////////////////////////////////////////////////////
int dockState = 0;

// 0 - needs setup
// 1 - connecting to wifi, turning on OTA
// 2 - successful connection
// 3 - normal operation, LED off, turns on when charging
// 4 - error
// 5 - LED brightness setup
// 6 - normal operation, remote fully charged
// 7 - normal operation, blinks to indicate remote is low battery

////////////////////////////////////////////////////////////////
// BLUETOOTH SETUP
////////////////////////////////////////////////////////////////
BluetoothSerial DOCK_BT;
String message = "";        // the message
bool recordmessage = false; // if true, bluetooth will start recording messages

////////////////////////////////////////////////////////////////
// WIFI SETUP
////////////////////////////////////////////////////////////////
#define CONN_TIMEOUT 60                      // wifi timeout
char hostString[] = "YIO-Dock-xxxxxxxxxxxx"; // stores the hostname
String ssid;                                 // ssid
String passwd;                               // password
String remote_id;                            // hostname of the remote
bool connected = false;
unsigned long WIFI_CHECK = 30000;

////////////////////////////////////////////////////////////////
// LED SETUP
////////////////////////////////////////////////////////////////
#define LED 23

// bool led_setup = false; // if the LED brightness is adjusted from the remote, this is true

int max_brightness = 50; // this updates from the remote settings screen

int led_delay = map(max_brightness, 5, 255, 30, 5);
int led_pause = map(max_brightness, 5, 255, 800, 0);

// setting PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 10; //Resolution 8, 10, 12, 15

TaskHandle_t LedTask; // core 0 task for handling LED stuff

////////////////////////////////////////////////////////////////
// CHARGING INDICATOR PIN
////////////////////////////////////////////////////////////////
#define CHG_PIN 13
bool charging = false;

void setCharging()
{
  Serial.print("CHG pin is: ");
  Serial.println(digitalRead(CHG_PIN));
  if (digitalRead(CHG_PIN) == LOW)
  {
    charging = true;

    // change dockstate back to normal charging when it was signalling low battery before
    if (dockState == 7) {
      dockState = 3;
    }
  }
  else
  {
    charging = false;
  }
}

////////////////////////////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////////////////////////////
void ledHandleTask(void *pvParameters)
{
  // LED setup
  pinMode(LED, OUTPUT);

  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(LED, ledChannel);

  while (1)
  {
    vTaskDelay(2 / portTICK_PERIOD_MS);

    // if the remote is charging, pulsate the LED
    // normal operation, LED off, turns on when charging
    if (dockState == 3 && charging)
    {
      led_delay = map(max_brightness, 5, 255, 30, 5);
      led_pause = map(max_brightness, 5, 255, 800, 0);

      for (int dutyCycle = 0; dutyCycle <= max_brightness; dutyCycle++)
      {
        // changing the LED brightness with PWM
        ledcWrite(ledChannel, dutyCycle);
        delay(led_delay);
      }

      delay(led_pause);

      // decrease the LED brightness
      for (int dutyCycle = max_brightness; dutyCycle >= 0; dutyCycle--)
      {
        // changing the LED brightness with PWM
        ledcWrite(ledChannel, dutyCycle);
        delay(led_delay);
      }
      delay(1000);
    }
    // needs setup
    else if (dockState == 0)
    {
      ledcWrite(ledChannel, 255);
      delay(800);
      ledcWrite(ledChannel, 0);
      delay(800);
    }
    // connecting to wifi, turning on OTA
    else if (dockState == 1)
    {
      ledcWrite(ledChannel, 255);
      delay(300);
      ledcWrite(ledChannel, 0);
      delay(300);
    }
    // successful connection
    else if (dockState == 2)
    {
      // Blink the LED 3 times to indicate successful connection
      for (int i = 0; i < 4; i++)
      {
        ledcWrite(ledChannel, 255);
        delay(100);
        ledcWrite(ledChannel, 0);
        delay(100);
      }
      dockState = 3;
    }
    // LED brightness setup
    else if (dockState == 5)
    {
      ledcWrite(ledChannel, max_brightness);
      delay(100);
    }
    // normal operation, remote fully charged
    else if (dockState == 6)
    {
      ledcWrite(ledChannel, max_brightness);
      delay(100);
    }
    // normal operation, blinks to indicate remote is low battery
    else if (dockState == 7)
    {
      for (int i = 0; i < 2; i++)
      {
        ledcWrite(ledChannel, max_brightness);
        delay(100);
        ledcWrite(ledChannel, 0);
        delay(100);
      }
      delay(4000);
    }
  }
}

void saveConfig(String data)
{
  DeserializationError error = deserializeJson(doc, data);

  if (error)
  {
    Serial.print(("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  // check if there is an SSID and password
  if (doc.containsKey("ssid") && doc.containsKey("password") && doc.containsKey("remote_id"))
  {
    ssid = doc["ssid"].as<String>();
    passwd = doc["password"].as<String>();
    remote_id = doc["remote_id"].as<String>();

    Preferences preferences;
    preferences.begin("Wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("passwd", passwd);
    preferences.putString("remote", remote_id);
    preferences.putBool("valid", true);
    preferences.end();
  }
  else if (doc.containsKey("erase"))
  { // or erease the preferences
    Preferences preferences;
    preferences.begin("Wifi", false);
    preferences.clear();
    preferences.end();

    int err;
    err = nvs_flash_init();
    Serial.println("nvs_flash_init: " + err);
    err = nvs_flash_erase();
    Serial.println("nvs_flash_erase: " + err);
  }

  ESP.restart();
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[WEBSOCKET] [%u] Disconnected!\n", num);

    // remove from connected clients
    for (int i = 0; i < serverClientCount; i++)
    {
      if (serverClients[i] == num)
      {
        serverClients[i] = -1;
      }
    }
    break;
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocketServer.remoteIP(num);
    Serial.printf("[WEBSOCKET] [%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

    // send auth request message
    StaticJsonDocument<200> responseDoc;
    responseDoc["type"] = "auth_required";
    String message;
    serializeJson(responseDoc, message);
    webSocketServer.sendTXT(num, message);
  }
  break;
  case WStype_TEXT:
  {
    // Serial.printf("[%u] get Text: %s\n", num, payload);
    deserializeJson(wsdoc, payload);

    // response json
    StaticJsonDocument<200> responseDoc;

    // API AUTHENTICATION
    if (wsdoc.containsKey("type") && wsdoc["type"].as<String>() == "auth")
    {
      if (wsdoc.containsKey("token"))
      {
        if (wsdoc["token"].as<String>() == token)
        {
          // token ok
          responseDoc["type"] = "auth_ok";
          String message;
          serializeJson(responseDoc, message);
          webSocketServer.sendTXT(num, message);

          // add client to authorized clients
          serverClients[serverClientCount] = num;
          serverClientCount++;

        }
        else
        {
          // invalid token
          responseDoc["type"] = "auth";
          responseDoc["message"] = "Invalid token";
          String message;
          serializeJson(responseDoc, message);
          webSocketServer.sendTXT(num, message);
        }
      }
      else
      {
        // token needed
        responseDoc["type"] = "auth";
        responseDoc["message"] = "Token needed";
        String message;
        serializeJson(responseDoc, message);
        webSocketServer.sendTXT(num, message);
      }
    }

    // COMMANDS
    for (int i = 0; i < serverClientCount; i++)
    {
      if (serverClients[i] != -1)
      {

        // it's on the list, let's see what it wants
        if (wsdoc.containsKey("type") && wsdoc["type"].as<String>() == "dock") {
                // Change LED brightness
                if (wsdoc["command"].as<String>() == "led_brightness_start") {
                    dockState = 5;
                    max_brightness = wsdoc["brightness"].as<int>();

                    Serial.println("[WEBSOCKET] Led brightness start");
                    Serial.print("Brightness: ");
                    Serial.println(max_brightness);
                }
                if (wsdoc["command"].as<String>() == "led_brightness_stop") {
                    dockState = 3;
                    ledcWrite(ledChannel, 0);

                    Serial.println("[WEBSOCKET] Led brightness stop");

                    // save settings
                    Preferences preferences;
                    preferences.begin("LED", false);
                    preferences.putInt("brightness", max_brightness);
                    preferences.end();
                }

                // Send IR code
                if (wsdoc["command"].as<String>() == "ir_send") {
                    Serial.println("[WEBSOCKET] IR Send");
                    irservice.send(wsdoc["code"].as<String>());
                }

                // Turn on IR receiving
                    if (wsdoc["command"].as<String>() == "ir_receive_on") {
                    irservice.receiving = true;
                    Serial.println("[WEBSOCKET] IR Receive on");    
                }

                // Turn off IR receiving
                if (wsdoc["command"].as<String>() == "ir_receive_off") {
                    irservice.receiving = false;
                    Serial.println("[WEBSOCKET] IR Receive off");       
                }

                // Change state to indicate remote is fully charged
                if (wsdoc["command"].as<String>() == "remote_charged") {
                    dockState = 6;      
                }

                // Change state to indicate remote is low battery
                if (wsdoc["command"].as<String>() == "remote_lowbattery") {
                    dockState = 7;      
                }

                // Erase and reset the dock
                if (wsdoc["command"].as<String>() == "reset") {
                    Serial.println("[WEBSOCKET] Reset");   
                    Preferences preferences;
                    preferences.begin("Wifi", false);
                    preferences.clear();
                    preferences.end();   

                    delay(500);
                    
                    preferences.begin("LED", false);
                    preferences.clear();
                    preferences.end();

                    int err;
                    err=nvs_flash_init();
                    Serial.println("nvs_flash_init: " + err);
                    err=nvs_flash_erase();
                    Serial.println("nvs_flash_erase: " + err);

                    delay(500);
                    ESP.restart();
                }
            }
      }
    }
  }
  break;

  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
  case WStype_BIN:
  case WStype_PING:
  case WStype_PONG:
  break;
  }
}


////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(115200);

  // run LED handling on other core
  xTaskCreatePinnedToCore(ledHandleTask, "LedTask", 10000, NULL, 1, &LedTask, 0);

  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  sprintf(hostString, "YIO-Dock-%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  Serial.println(String(hostString));

  // CHARGING PIN setup
  pinMode(CHG_PIN, INPUT);
  attachInterrupt(CHG_PIN, setCharging, CHANGE);

  // check if there are ssid and password stored
  Preferences preferences;
  preferences.begin("Wifi", false);
  bool hasPref = preferences.getBool("valid", false);

  if (hasPref)
  {
    ssid = preferences.getString("ssid", "");
    passwd = preferences.getString("passwd", "");
    remote_id = preferences.getString("remote", "");

    if (ssid.equals("") || passwd.equals("") || remote_id.equals(""))
    {
      Serial.println("Invalid credidentials");
    }
    else
    {
      // needsSetup = false;
      dockState = 1;
    }
  }

  preferences.end();

  delay(200);

  // Getting LED brightness
  preferences.begin("LED", false);
  int led_brightness = preferences.getInt("brightness", 0);

  if (led_brightness == 0)
  {
    // no settings stored yet. Setting default
    Serial.println("No LED brightness stored, setting default");
    preferences.putInt("brightness", max_brightness);
  }
  else
  {
    Serial.print("LED brightness setting found: ");
    Serial.println(led_brightness);
    max_brightness = led_brightness;
  }

  preferences.end();

  // start bluetooth server
  if (DOCK_BT.begin(hostString))
  {
    Serial.println("Bluetooth successful");
  }

  delay(1000);

  if (dockState != 0)
  { // connect to the Wifi network
    Serial.println("Connecting to wifi");

    WiFi.disconnect();
    delay(1000);
    WiFi.enableSTA(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), passwd.c_str());
    
    int connCounter = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(300);
      Serial.print(".");
      connCounter += 1;

      // if cannot connect, go back to bluetooth server mode
      if (connCounter >= CONN_TIMEOUT)
      {
        Serial.println("Connection timeout");
        // Preferences preferences;
        // preferences.begin("Wifi", false);
        // preferences.clear();
        // preferences.end();

        ESP.restart();
      }
    }

    Serial.println("Successful connection");
    Serial.print("Got ip: ");
    Serial.println(WiFi.localIP());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());

    if (!MDNS.begin(hostString))
    {
      Serial.println("Error setting up MDNS responder!");
      while (1)
      {
        delay(1000);
      }
    }
    Serial.println("mDNS started");

    // Add mDNS service
    MDNS.addService("yio-dock-ota", "tcp", 80);
    MDNS.addService("yio-dock-api", "tcp", 946);

    // initialize the OTA service
    ota.init();

    // initialize the IR service
    irservice.init();

    // start websocket API
    webSocketServer.begin();
    webSocketServer.onEvent(webSocketEvent);

    dockState = 2;
  }
}

////////////////////////////////////////////////////////////////
// LOOP
////////////////////////////////////////////////////////////////
void loop()
{
  // wifi reconnect
  if (WiFi.status() != WL_CONNECTED && (millis() > WIFI_CHECK)) {
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(ssid.c_str(), passwd.c_str());
    WIFI_CHECK = millis() + 30000;
  }

  // look for wifi credidentials on bluetooth when in setup mode
  if (DOCK_BT.available() != 0)
  {
    char incomingChar = DOCK_BT.read();
    if (String(incomingChar) == "{")
    {
      recordmessage = true;
      message = "";
    }
    else if (String(incomingChar) == "}")
    {
      recordmessage = false;
      message += "}";
      saveConfig(message);
    }
    if (recordmessage)
    {
      message += String(incomingChar);
    }
  }

  if (dockState > 0)
  {
    // OTA update
    ota.handle();

    // websocket server loop
    webSocketServer.loop();

    //IR Receive
    if (irservice.receiving)
    {
        String code_received = irservice.receive();

        if (code_received != "") {
        StaticJsonDocument<500> responseDoc;
        responseDoc["type"] = "dock";
        responseDoc["command"] = "ir_receive";
        responseDoc["code"] = code_received;

        String message;
        serializeJson(responseDoc, message);
        webSocketServer.broadcastTXT(message);
        Serial.println(message);
        Serial.println("OK");    
        }
    }
  }

  delay(100);
}