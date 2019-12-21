#include "service_mdns.h"
#include <config.h>

MDNSService* MDNSService::s_instance = nullptr;

MDNSService::MDNSService()
{
    s_instance = this;
}

void MDNSService::init()
{
    if (!MDNS.begin(Config::getInstance()->getHostName().c_str()))
    {
        Serial.println(F("Error setting up MDNS responder!"));
        while (1)
        {
        delay(1000);
        }
    }
    Serial.println(F("mDNS started"));

    // Add mDNS service
    MDNS.addService("yio-dock-ota", "tcp", Config::getInstance()->OTA_port);
    MDNS.addService("yio-dock-api", "tcp", Config::getInstance()->API_port);
    MDNS.addServiceTxt("yio-dock-api", "tcp", "dockFriendlyName", Config::getInstance()->getFriendlyName());
}