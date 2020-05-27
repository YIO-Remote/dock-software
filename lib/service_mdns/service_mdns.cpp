#include "service_mdns.h"

MDNSService* MDNSService::s_instance = nullptr;

MDNSService::MDNSService()
{
    s_instance = this;
}

void MDNSService::loop()
{
    const unsigned long fiveMinutes = 1 * 60 * 1000UL;
    static unsigned long lastSampleTime = 0 - fiveMinutes;

    unsigned long now = millis();
    if (now - lastSampleTime >= fiveMinutes)
    {
        lastSampleTime += fiveMinutes;
        MDNS.end();
        delay(100);

        if (!MDNS.begin(m_config->getHostName().c_str()))
        {
            Serial.println(F("[MDNS] Error setting up MDNS responder!"));
            while (1)
            {
            delay(1000);
            }
        }
        Serial.println(F("[MDNS] mDNS started"));

        // Add mDNS service
        MDNS.addService("_yio-dock-ota", "_tcp", m_config->OTA_port);
        MDNS.addService("_yio-dock-api", "_tcp", m_config->API_port);
        addFriendlyName(m_config->getFriendlyName());
        Serial.println(F("[MDNS] Services updated"));
    }
}

void MDNSService::addFriendlyName(String name)
{
    MDNS.addServiceTxt("_yio-dock-api", "_tcp", "FriendlyName", name);
}