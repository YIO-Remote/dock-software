#include <Arduino.h>
#include <config.h>
#include <led_control.h>
#include <state.h>
#include <service_ota.h>
#include <service_ir.h>
#include <service_wifi.h>
#include <service_blueooth.h>
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
BluetoothService* bluetoothService;
MDNSService* mdnsService;
OTA otaService;
API* api;
InfraredService* irService;

bool resetMarker = false;

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

    if (elapsedTimeInMiliSeconds > 3000 && elapsedTimeInMiliSeconds < 10000) // between 5 and 20 seconds.
    {
      resetMarker = true;
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
  bluetoothService = new BluetoothService();
  irService = new InfraredService();
  api = new API();
  mdnsService = new MDNSService();

  if (config->getWifiSsid() != "") {
    state->currentState = State::CONNECTING;
    Serial.println(F("[MAIN] SSID found, connecting..."));
  }

  // initialize Bluetooth
  if (state->currentState == State::SETUP) {
    bluetoothService->init();
  } else {
    // CHARGING PIN setup
    setupChargingPin();

    // BUTTON PIN setup
    setupButtonPin();

    // initiate WiFi
    wifiService->initiateWifi();

    // initialize API service
    api->init();

    // initialize OTA service
    otaService.init();

    // initialize IR service
    irService->init();
  }
}

////////////////////////////////////////////////////////////////
// Main LOOP
////////////////////////////////////////////////////////////////
void loop()
{
  if (state->currentState == State::SETUP) {
    // Handle incoming bluetooth serial data
    bluetoothService->handle();
  } else {
    // Handle wifi disconnects.
    wifiService->handleReconnect();

    // Handle api calls
    api->loop();

    // handle IR
    irService->loop();

    // handle MDNS
    mdnsService->loop();

    // Handle OTA updates.
    otaService.handle();

    // reset if marker is set
    if (resetMarker) {
      config->reset();
    }
  }
}