#include <Arduino.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"
#include "SPIFFS.h"
#include <WebSocketsServer.h>

#include <config.h>
#include <led_control.h>
#include <state.h>
#include <service_ota.h>
#include <service_ir.h>
#include <service_wifi.h>
#include <service_mdns.h>

#define CHARGING_GPIO 13
#define BUTTON_GPIO 0

// Services
Config* config;
LedControl* ledControl;
State* state;
WifiService* wifiservice;
MDNSService* mdnsservice;
OTA* otaService;
InfraredService irService;
WebSocketsServer webSocketServer = WebSocketsServer(config->API_port);
BluetoothSerial bluetoothService;

TaskHandle_t LedTask;        // task for handling LED stuff
TaskHandle_t BTSettingsTask; // task for handling BluetoothTasks

// Global Vars
uint8_t webSocketClients[100] = {};
int webSocketClientsCount = 0;

bool initiatFactoryReset = false;

const int64_t TIMER_RESET_TIME = 9223372036854775807;
int64_t buttonTimerSet = TIMER_RESET_TIME;

char dockHostName[] = "YIO-Dock-xxxxxxxxxxxx"; // stores the hostname
String dockFriendlyName = "YIO-Dock-xxxxxxxxxxxx";

void setCharging()
{
  Serial.print(F("CHG pin is: "));
  Serial.println(digitalRead(CHARGING_GPIO));
  if (digitalRead(CHARGING_GPIO) == LOW)
  {
    state->currentState = State::NORMAL_CHARGING;

    // change state->currentState back to normal charging when it was signalling low battery before
    if (state->currentState == State::NORMAL_LOWBATTERY)
    {
      state->currentState = State::NORMAL_CHARGING;
    }
  }
  else
  {
    state->currentState = State::NORMAL;
  }
}

////////////////////////////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////////////////////////////
void ledHandleTask(void *pvParameters)
{
  ledControl->loop();
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
    config->setWifiSsid(wifiJsonDocument["ssid"].as<String>());
    config->setWifiPassword(wifiJsonDocument["password"].as<String>());

    Serial.println("Saving SSID:" + config->getWifiSsid() + " PASS:" + config->getWifiPassword());

    Serial.println(F("Disconnecting any current WiFi connections."));
    WiFi.disconnect();
    delay(1000);
    Serial.println(F("Connecting to provided WiFi credentials."));
    wifiservice->connect(config->getWifiSsid(), config->getWifiPassword());
  }

  state->reboot();
}

void BTSettingsHandleTask(void *pvParameters)
{
  // Wait two seconds before actually enabling bluetooth.
  // In normal operation the function is closed within a couple of miliseconds. and there's no need to initialize anything.
  delay(2000);

  String receivedBtData = "";
  bool interestingData = false; // Data between { and } chars.

  //Initiate Bluetooth
  if (bluetoothService.begin(config->getHostName()))
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
        if (webSocketJsonDocument["token"].as<String>() == config->token)
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
            state->currentState = State::LED_SETUP;
            ledMaxBrightness = webSocketJsonDocument["brightness"].as<int>();

            Serial.println(F("[WEBSOCKET] Led brightness start"));
            Serial.print(F("Brightness: "));
            Serial.println(ledMaxBrightness);
          }
          if (webSocketJsonDocument["command"].as<String>() == "led_brightness_stop")
          {
            state->currentState = State::NORMAL;
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
            state->currentState = State::NORMAL_FULLYCHARGED;
          }

          // Change state to indicate remote is low battery
          if (webSocketJsonDocument["command"].as<String>() == "remote_lowbattery")
          {
            state->currentState = State::NORMAL_LOWBATTERY;
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

void setupChargingPin()
{
  pinMode(CHARGING_GPIO, INPUT);
  attachInterrupt(CHARGING_GPIO, setCharging, CHANGE);
  if (digitalRead(CHARGING_GPIO) == LOW) // if there's a remote already charging, turn on charging
  {
    state->currentState = State::NORMAL_CHARGING;
  }
}

void handleFactoryReset()
{
  if (initiatFactoryReset)
  {
    config->reset();

    Serial.println(F("Resetting WiFi credentials."));
    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    if (esp_wifi_set_config(WIFI_IF_STA, &conf))
    {
      log_e("clear config failed!");
    }

    state->reboot();
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
  state->printDockInfo();

  // Run bluetooth serial config thread
  xTaskCreatePinnedToCore(BTSettingsHandleTask, "BTSettingsTask", 10000, NULL, 1, &BTSettingsTask, 0);
  // Run LED handling on other core
  xTaskCreatePinnedToCore(ledHandleTask, "LedTask", 10000, NULL, 1, &LedTask, 0);

  // initiate WiFi/WifiManager
  wifiservice->initiateWifi();

  //Killing Bluetooth task when connected.
  vTaskDelete(BTSettingsTask);

  // CHARGING PIN setup
  setupChargingPin();

  // BUTTON PIN setup
  setupButtonPin();

  // Set the max brightness from saved settings
  ledControl->setLedMaxBrightness(config->getLedBrightness());

  // initialize OTA service
  otaService->init();

  // initialize MDNS
  mdnsservice->init();

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
  wifiservice->handleReconnect();

  // Handle OTA updates.
  otaService->handle();

  // Handle websocket connections.
  webSocketServer.loop();

  // Handle received IR codes.
  handleIrReceive();

  // Handle Factory reset
  handleFactoryReset();

  // Time to rest
  delay(100);
}