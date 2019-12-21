#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"
#include <Preferences.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include <service_ota.h>
#include <service_ir.h>
#include <WebSocketsServer.h>

#include <config.h>
#include <led_control.h>
#include <state.h>

// Constants.
const String BT_ACCESS_TOKEN = "0";
const int WS_SERVICE_PORT = 946;
const int OTA_SERVICE_PORT = 80;
const int LED_PWM_FREQ = 5000;
const int LED_CHANNEL = 0;
const int LED_RESOLUTION = 8; //Resolution 8, 10, 12, 15
#define LED_GPIO 23
#define CHARGING_GPIO 13
#define BUTTON_GPIO 0

// Services
Config config;
LedControl ledControl;

OTA otaService;
InfraredService irService;
WebSocketsServer webSocketServer = WebSocketsServer(WS_SERVICE_PORT);
BluetoothSerial bluetoothService;
TaskHandle_t LedTask;        // task for handling LED stuff
TaskHandle_t BTSettingsTask; // task for handling BluetoothTasks

// Global Vars
uint8_t webSocketClients[100] = {};
int webSocketClientsCount = 0;

String wifiSSID;            // ssid
String wifiPSK;             // password
bool wifiPrevState = false; // previous WIFI connection state; 0 - disconnected, 1 - connected
unsigned long wifiCheckTimedUl = 30000;

int ledMaxBrightness = 50; // this updates from the remote settings screen
int ledMapDelay = map(ledMaxBrightness, 5, 255, 30, 5);
int ledMapPause = map(ledMaxBrightness, 5, 255, 800, 0);

bool initiatFactoryReset = false;
bool chargingState = false;

const int64_t TIMER_RESET_TIME = 9223372036854775807;
int64_t buttonTimerSet = TIMER_RESET_TIME;

char dockHostName[] = "YIO-Dock-xxxxxxxxxxxx"; // stores the hostname
String dockFriendlyName = "YIO-Dock-xxxxxxxxxxxx";
int dockState = 0;
// 0 - needs setup
// 1 - connecting to wifi, turning on OTA
// 2 - successful connection
// 3 - normal operation, LED off, turns on when charging
// 4 - error
// 5 - LED brightness setup
// 6 - normal operation, remote fully charged
// 7 - normal operation, blinks to indicate remote is low battery

void setCharging()
{
  Serial.print(F("CHG pin is: "));
  Serial.println(digitalRead(CHARGING_GPIO));
  if (digitalRead(CHARGING_GPIO) == LOW)
  {
    chargingState = true;

    // change dockstate back to normal charging when it was signalling low battery before
    if (dockState == 7)
    {
      dockState = 3;
    }
  }
  else
  {
    chargingState = false;
  }
}

////////////////////////////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////////////////////////////
void ledHandleTask(void *pvParameters)
{
  // LED setup
  pinMode(LED_GPIO, OUTPUT);

  ledcSetup(LED_CHANNEL, LED_PWM_FREQ, LED_RESOLUTION);
  ledcAttachPin(LED_GPIO, LED_CHANNEL);

  while (1)
  {
    vTaskDelay(2 / portTICK_PERIOD_MS);

    // if the remote is charging, pulsate the LED
    // normal operation, LED off, turns on when charging
    if (dockState == 3 && chargingState)
    {
      ledMapDelay = map(ledMaxBrightness, 5, 255, 30, 5);
      ledMapPause = map(ledMaxBrightness, 5, 255, 800, 0);

      for (int dutyCycle = 0; dutyCycle <= ledMaxBrightness; dutyCycle++)
      {
        // changing the LED brightness with PWM
        ledcWrite(LED_CHANNEL, dutyCycle);
        delay(ledMapDelay);
      }

      delay(ledMapPause);

      // decrease the LED brightness
      for (int dutyCycle = ledMaxBrightness; dutyCycle >= 0; dutyCycle--)
      {
        // changing the LED brightness with PWM
        ledcWrite(LED_CHANNEL, dutyCycle);
        delay(ledMapDelay);
      }
      delay(1000);
    }
    // needs setup
    else if (dockState == 0)
    {
      ledcWrite(LED_CHANNEL, 255);
      delay(800);
      ledcWrite(LED_CHANNEL, 0);
      delay(800);
    }
    // connecting to wifi, turning on OTA
    else if (dockState == 1)
    {
      ledcWrite(LED_CHANNEL, 255);
      delay(300);
      ledcWrite(LED_CHANNEL, 0);
      delay(300);
    }
    // successful connection
    else if (dockState == 2)
    {
      // Blink the LED 3 times to indicate successful connection
      for (int i = 0; i < 4; i++)
      {
        ledcWrite(LED_CHANNEL, 255);
        delay(100);
        ledcWrite(LED_CHANNEL, 0);
        delay(100);
      }
      dockState = 3;
    }
    // LED brightness setup
    else if (dockState == 5)
    {
      ledcWrite(LED_CHANNEL, ledMaxBrightness);
      delay(100);
    }
    // normal operation, remote fully charged
    else if (dockState == 6)
    {
      ledcWrite(LED_CHANNEL, ledMaxBrightness);
      delay(100);
    }
    // normal operation, blinks to indicate remote is low battery
    else if (dockState == 7)
    {
      for (int i = 0; i < 2; i++)
      {
        ledcWrite(LED_CHANNEL, ledMaxBrightness);
        delay(100);
        ledcWrite(LED_CHANNEL, 0);
        delay(100);
      }
      delay(4000);
    }
  }
}

void rebootDock()
{
  Serial.println(F("About to reboot..."));
  delay(2000);
  Serial.println(F("Now rebooting..."));
  ESP.restart();
}

void saveWiFiJsonToESP(String data)
{
  StaticJsonDocument<200> wifiJsonDocument;
  DeserializationError error = deserializeJson(wifiJsonDocument, data);
  WiFiManager wifiManager;

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  if (wifiJsonDocument.containsKey("ssid") && wifiJsonDocument.containsKey("password"))
  {
    wifiSSID = wifiJsonDocument["ssid"].as<String>();
    wifiPSK = wifiJsonDocument["password"].as<String>();

    Serial.println("Saving SSID:" + wifiSSID + " PASS:" + wifiPSK);

    Serial.println(F("Disconnecting any current WiFi connections."));
    WiFi.disconnect();
    delay(1000);
    Serial.println(F("Connecting to provided WiFi credentials."));
    wifiManager.connectWifi(wifiSSID, wifiPSK);
  }

  rebootDock();
}

void BTSettingsHandleTask(void *pvParameters)
{
  // Wait two seconds before actually enabling bluetooth.
  // In normal operation the function is closed within a couple of miliseconds. and there's no need to initialize anything.
  delay(2000);

  String receivedBtData = "";
  bool interestingData = false; // Data between { and } chars.

  //Initiate Bluetooth
  if (bluetoothService.begin(dockHostName))
  {
    Serial.println(F("Bluetooth configurator started."));
  }

  for (;;) //Endless loop
  {
    char incomingChar = bluetoothService.read();
    int charAsciiNumber = incomingChar + 0;
    if (charAsciiNumber != 255) //ASCII 255 is continually send
    {
      bluetoothService.print(incomingChar); //Echo send characters.
    }

    if (String(incomingChar) == "{")
    {
      interestingData = true;
      receivedBtData = "";
    }
    else if (String(incomingChar) == "}")
    {
      interestingData = false;
      receivedBtData += "}";
      saveWiFiJsonToESP(receivedBtData);
    }
    if (interestingData)
    {
      receivedBtData += String(incomingChar);
    }
    delay(10);
  }
}

void preferencesReset()
{
  Preferences preferences;

  preferences.begin("LED", false);
  preferences.clear();
  preferences.end();

  delay(500);

  preferences.begin("general", false);
  preferences.clear();
  preferences.end();

  int err;
  err = nvs_flash_init();
  Serial.println("nvs_flash_init: " + err);
  err = nvs_flash_erase();
  Serial.println("nvs_flash_erase: " + err);

  rebootDock();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[WEBSOCKET] [%u] Disconnected!\n", num);

    // remove from connected clients
    for (int i = 0; i < webSocketClientsCount; i++)
    {
      if (webSocketClients[i] == num)
      {
        webSocketClients[i] = -1;
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
    StaticJsonDocument<200> webSocketJsonDocument;
    deserializeJson(webSocketJsonDocument, payload);

    // response json
    StaticJsonDocument<200> responseDoc;

    // API AUTHENTICATION
    if (webSocketJsonDocument.containsKey("type") && webSocketJsonDocument["type"].as<String>() == "auth")
    {
      if (webSocketJsonDocument.containsKey("token"))
      {
        if (webSocketJsonDocument["token"].as<String>() == BT_ACCESS_TOKEN)
        {
          // token ok
          responseDoc["type"] = "auth_ok";
          String message;
          serializeJson(responseDoc, message);
          webSocketServer.sendTXT(num, message);

          // add client to authorized clients
          webSocketClients[webSocketClientsCount] = num;
          webSocketClientsCount++;
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
    for (int i = 0; i < webSocketClientsCount; i++)
    {
      if (webSocketClients[i] != -1)
      {

        // it's on the list, let's see what it wants
        if (webSocketJsonDocument.containsKey("type") && webSocketJsonDocument["type"].as<String>() == "dock")
        {
          // Change LED brightness
          if (webSocketJsonDocument["command"].as<String>() == "led_brightness_start")
          {
            dockState = 5;
            ledMaxBrightness = webSocketJsonDocument["brightness"].as<int>();

            Serial.println(F("[WEBSOCKET] Led brightness start"));
            Serial.print(F("Brightness: "));
            Serial.println(ledMaxBrightness);
          }
          if (webSocketJsonDocument["command"].as<String>() == "led_brightness_stop")
          {
            dockState = 3;
            ledcWrite(LED_CHANNEL, 0);

            Serial.println(F("[WEBSOCKET] Led brightness stop"));

            // save settings
            Preferences preferences;
            preferences.begin("LED", false);
            preferences.putInt("brightness", ledMaxBrightness);
            preferences.end();
          }

          // Send IR code
          if (webSocketJsonDocument["command"].as<String>() == "ir_send")
          {
            Serial.println(F("[WEBSOCKET] IR Send"));
            irService.send(webSocketJsonDocument["code"].as<String>());
          }

          // Turn on IR receiving
          if (webSocketJsonDocument["command"].as<String>() == "ir_receive_on")
          {
            irService.receiving = true;
            Serial.println(F("[WEBSOCKET] IR Receive on"));
          }

          // Turn off IR receiving
          if (webSocketJsonDocument["command"].as<String>() == "ir_receive_off")
          {
            irService.receiving = false;
            Serial.println(F("[WEBSOCKET] IR Receive off"));
          }

          // Change state to indicate remote is fully charged
          if (webSocketJsonDocument["command"].as<String>() == "remote_charged")
          {
            dockState = 6;
          }

          // Change state to indicate remote is low battery
          if (webSocketJsonDocument["command"].as<String>() == "remote_lowbattery")
          {
            dockState = 7;
          }

          // Change friendly name
          if (webSocketJsonDocument["command"].as<String>() == "set_friendly_name")
          {
            dockFriendlyName = webSocketJsonDocument["set_friendly_name"].as<String>();

            Preferences preferences;
            preferences.begin("general", false);
            preferences.putString("friendly_name", dockFriendlyName);
            preferences.end();

            MDNS.addServiceTxt("yio-dock-api", "tcp", "dockFriendlyName", dockFriendlyName);
          }

          // Erase and reset the dock
          if (webSocketJsonDocument["command"].as<String>() == "reset")
          {
            Serial.println(F("[WEBSOCKET] Reset"));
            initiatFactoryReset = true;
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

void mDNSInit()
{
  if (!MDNS.begin(dockHostName))
  {
    Serial.println(F("Error setting up MDNS responder!"));
    while (1)
    {
      delay(1000);
    }
  }
  Serial.println(F("mDNS started"));

  // Add mDNS service
  MDNS.addService("yio-dock-ota", "tcp", OTA_SERVICE_PORT);
  MDNS.addService("yio-dock-api", "tcp", WS_SERVICE_PORT);
  MDNS.addServiceTxt("yio-dock-api", "tcp", "dockFriendlyName", dockFriendlyName);
}

void buildHostname()
{
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  sprintf(dockHostName, "YIO-Dock-%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  Serial.print(F("Hostmane: "));
  Serial.println(String(dockHostName));
}

void printDockInfo()
{
  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("############################################################"));
  Serial.println(F("## YIO Dock firmware                                      ##"));
  Serial.println(F("## Visit http://yio-remote.com/ for more information      ##"));
  Serial.println(F("############################################################"));
  Serial.println(F(""));
}

void handlePreferances()
{
  Preferences preferences;
  preferences.begin("LED", false);
  int led_brightness = preferences.getInt("brightness", 0);

  if (led_brightness == 0) // no settings stored yet. Setting default
  {
    Serial.println(F("No LED brightness stored, setting default"));
    preferences.putInt("brightness", ledMaxBrightness);
  }
  else
  {
    Serial.print(F("LED brightness setting found: "));
    Serial.println(led_brightness);
    ledMaxBrightness = led_brightness;
  }
  preferences.end();

  // Getting friendly name
  preferences.begin("general", false);
  dockFriendlyName = preferences.getString("friendly_name", "");
  if (dockFriendlyName.equals("")) // no settings stored yet. Setting default
  {
    dockFriendlyName = dockHostName;
    preferences.putString("friendly_name", dockHostName);
  }
  else // Settings found.
  {
    Serial.print(F("Friendly name setting found: "));
    Serial.println(dockFriendlyName);
  }
  preferences.end();
}

void handleWifiReconnect()
{
  if (WiFi.status() != WL_CONNECTED && (millis() > wifiCheckTimedUl))
  {
    wifiPrevState = false;
    MDNS.end();

    WiFi.disconnect();
    delay(1000);
    WiFi.enableSTA(true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(wifiSSID.c_str(), wifiPSK.c_str());
    wifiCheckTimedUl = millis() + 30000;
  }

  // restart MDNS if wifi is connected again
  if (WiFi.status() == WL_CONNECTED && wifiPrevState == false)
  {
    wifiPrevState = true;
    mDNSInit();
  }
}

void handleIrReceive()
{
  //IR Receive
  if (irService.receiving)
  {
    String code_received = irService.receive();

    if (code_received != "")
    {
      StaticJsonDocument<500> responseDoc;
      responseDoc["type"] = "dock";
      responseDoc["command"] = "ir_receive";
      responseDoc["code"] = code_received;

      String message;
      serializeJson(responseDoc, message);
      webSocketServer.broadcastTXT(message);
      Serial.println(message);
      Serial.println(F("OK"));
    }
  }
}

void initiateWiFi()
{
  WiFiManager wifiManager; // use wifiManager.resetSettings(); //resetting saved wifi credentials.
  wifiManager.autoConnect(dockHostName);
  wifiSSID = wifiManager.getSSID();
  wifiPSK = wifiManager.getPassword();
  wifiPrevState = true;
  dockState = 2;
}

void setupChargingPin()
{
  pinMode(CHARGING_GPIO, INPUT);
  attachInterrupt(CHARGING_GPIO, setCharging, CHANGE);
  if (digitalRead(CHARGING_GPIO) == LOW) // if there's a remote already charging, turn on charging
  {
    chargingState = true;
  }
}

void handleFactoryReset()
{
  if (initiatFactoryReset)
  {
    preferencesReset();

    Serial.println(F("Resetting WiFi credentials."));
    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    if (esp_wifi_set_config(WIFI_IF_STA, &conf))
    {
      log_e("clear config failed!");
    }

    rebootDock();
  }
}

void handleButtonPress()
{
  const int64_t timerCurrent = esp_timer_get_time();
  if (digitalRead(BUTTON_GPIO) == LOW)
  {
    buttonTimerSet = timerCurrent;
    Serial.print(F("Button is pressed."));
  }
  else
  {
    const int elapsedTimeInMiliSeconds = ((timerCurrent - buttonTimerSet) / 1000);
    Serial.print(F("Button held for "));
    Serial.print(elapsedTimeInMiliSeconds);
    Serial.println(F(" mili seconds."));
    buttonTimerSet = TIMER_RESET_TIME;

    if (elapsedTimeInMiliSeconds > 5000 && elapsedTimeInMiliSeconds < 20000) // between 5 and 20 seconds.
    {
      initiatFactoryReset = true;
    }
  }
}

void setupButtonPin()
{
  pinMode(BUTTON_GPIO, INPUT);
  attachInterrupt(BUTTON_GPIO, handleButtonPress, CHANGE);
}

////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(115200);

  //Print Dock Info.
  printDockInfo();

  // Form hostname
  buildHostname();

  // Run bluetooth serial config thread
  xTaskCreatePinnedToCore(BTSettingsHandleTask, "BTSettingsTask", 10000, NULL, 1, &BTSettingsTask, 0);
  // Run LED handling on other core
  //xTaskCreatePinnedToCore(ledHandleTask, "LedTask", 10000, NULL, 1, &LedTask, 0);
  xTaskCreatePinnedToCore(ledControl.loop, "LedTask", 10000, NULL, 1, &LedTask, 0);

  // initiate WiFi/WifiManager
  initiateWiFi();

  //Killing Bluetooth task when connected.
  vTaskDelete(BTSettingsTask);

  // CHARGING PIN setup
  setupChargingPin();

  // BUTTON PIN setup
  setupButtonPin();

  // Handle stored Preferences
  handlePreferances();

  // initialize OTA service
  otaService.init();

  // initialize MDNS
  mDNSInit();

  // initialize IR service
  irService.init();

  // start websocket API
  webSocketServer.begin();
  webSocketServer.onEvent(webSocketEvent);
}

////////////////////////////////////////////////////////////////
// Main LOOP
////////////////////////////////////////////////////////////////
void loop()
{
  // Handle wifi disconnects.
  handleWifiReconnect();

  // Handle OTA updates.
  otaService.handle();

  // Handle websocket connections.
  webSocketServer.loop();

  // Handle received IR codes.
  handleIrReceive();

  // Handle Factory reset
  handleFactoryReset();

  // Time to rest
  delay(100);
}