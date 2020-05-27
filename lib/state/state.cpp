#include <Arduino.h>
#include "state.h"
#include "config.h"

State* State::s_instance = nullptr;

State::State()
{
    s_instance = this;

    printDockInfo();
}

void State::reboot()
{
    Serial.println(F("About to reboot..."));
    delay(2000);
    Serial.println(F("Now rebooting..."));
    ESP.restart();
}

void State::printDockInfo()
{
    Serial.println(F(""));
    Serial.println(F(""));
    Serial.println(F("############################################################"));
    Serial.println(F("## YIO Dock firmware                                      ##"));
    Serial.println(F("## Visit http://yio-remote.com/ for more information      ##"));
    Serial.println(F("############################################################"));
    Serial.println(F(""));
}