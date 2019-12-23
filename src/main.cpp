#include <Arduino.h>
#include "BluetoothSerial.h"
#include <config.h>
#include <led_control.h>
#include <state.h>
#include <service_ota.h>
#include <service_ir.h>
#include <service_wifi.h>
#include <service_mdns.h>
#include <service_api.h>

// PIN SETUP
// Indicator LED, IR receiver and IR LED pins are setup in the corresponding classes
#define CHARGING_GPIO 13
#define BUTTON_GPIO 0

// Services
Config* config;
LedControl* ledControl;
State* state;
WifiService* wifiService;
MDNSService* mdnsService;
OTA otaService;
API* api;
InfraredService* irService;
BluetoothSerial bluetoothService;

TaskHandle_t BTSettingsTask; // task for handling BluetoothTasks

////////////////////////////////////////////////////////////////
// THREADED FUNCTIONS
////////////////////////////////////////////////////////////////
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
      api->processData(receivedBtData, 0, "bluetooth");
    }
    if (interestingData)
    {
      receivedBtData += String(incomingChar);
    }
    delay(10);
  }
}

////////////////////////////////////////////////////////////////
// INTERRUPT SETUPS
////////////////////////////////////////////////////////////////
// charging pin interrupt callback
void setCharging()
{
  Serial.print(F("[MAIN] CHG pin is: "));
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

// charging pin
const int64_t TIMER_RESET_TIME = 9223372036854775807;
int64_t buttonTimerSet = TIMER_RESET_TIME;

void setupChargingPin()
{
  pinMode(CHARGING_GPIO, INPUT);
  attachInterrupt(CHARGING_GPIO, setCharging, CHANGE);
  if (digitalRead(CHARGING_GPIO) == LOW) // if there's a remote already charging, turn on charging
  {
    state->currentState = State::NORMAL_CHARGING;
  }
}

// button press interrupt callback
void handleButtonPress()
{
  const int64_t timerCurrent = esp_timer_get_time();
  if (digitalRead(BUTTON_GPIO) == LOW)
  {
    buttonTimerSet = timerCurrent;
    Serial.print(F("[MAIN] Button is pressed."));
  }
  else
  {
    const int elapsedTimeInMiliSeconds = ((timerCurrent - buttonTimerSet) / 1000);
    Serial.print(F("[MAIN] Button held for "));
    Serial.print(elapsedTimeInMiliSeconds);
    Serial.println(F(" mili seconds."));
    buttonTimerSet = TIMER_RESET_TIME;

    if (elapsedTimeInMiliSeconds > 5000 && elapsedTimeInMiliSeconds < 20000) // between 5 and 20 seconds.
    {
      config->reset();
    }
  }
}

// GPIO button pin
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

  config = new Config();
  state = new State();
  ledControl = new LedControl();
  ledControl->setLedMaxBrightness(config->getLedBrightness());
  wifiService = new WifiService();
  irService = new InfraredService();
  api = new API();
  mdnsService = new MDNSService();


  //Print Dock Info.
  state->printDockInfo();

  // Run bluetooth serial config thread
  xTaskCreatePinnedToCore(BTSettingsHandleTask, "BTSettingsTask", 10000, NULL, 1, &BTSettingsTask, 0);

  // initiate WiFi/WifiManager
  wifiService->initiateWifi();

  //Killing Bluetooth task when connected.
  vTaskDelete(BTSettingsTask);

  // CHARGING PIN setup
  setupChargingPin();

  // BUTTON PIN setup
  setupButtonPin();

  // initialize API service
  api->init();

  // initialize MDNS service
  mdnsService->init();

  // initialize OTA service
  otaService.init();

  // initialize IR service
  irService->init();
}

////////////////////////////////////////////////////////////////
// Main LOOP
////////////////////////////////////////////////////////////////
void loop()
{
  // Handle wifi disconnects.
  wifiService->handleReconnect();

  // Handle OTA updates.
  otaService.handle();

  // Handle api calls
  api->loop();

  // handle IR
  irService->loop();

  // handle MDNS
  mdnsService->loop();

  // Time to rest
  delay(100);
}