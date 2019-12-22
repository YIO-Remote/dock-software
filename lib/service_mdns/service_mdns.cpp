#include "service_mdns.h"

MDNSService* MDNSService::s_instance = nullptr;

MDNSService::MDNSService()
{
    s_instance = this;
}

void MDNSService::init()
{
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
    MDNS.addService("yio-dock-ota", "tcp", m_config->OTA_port);
    MDNS.addService("yio-dock-api", "tcp", m_config->API_port);
    MDNS.addServiceTxt("yio-dock-api", "tcp", "dockFriendlyName", m_config->getFriendlyName());
    Serial.println(F("[MDNS] Services added"));
}