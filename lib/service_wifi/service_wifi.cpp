#include "service_wifi.h"

WifiService* WifiService::s_instance = nullptr;

WifiService::WifiService()
{
    s_instance = this;
}

void WifiService::initiateWifi()
{
    Serial.println(F("[WIFI] Initializing..."));
    connect(m_config->getWifiSsid(), m_config->getWifiPassword());
}

void WifiService::handleReconnect()
{
    if (WiFi.status() != WL_CONNECTED && (millis() > m_wifiCheckTimedUl))
    {
        m_wifiReconnectCount++;

        if (m_wifiReconnectCount == 5) {
            m_wifiReconnectCount = 0;
            m_state->reboot();
        }

        Serial.println(F("[WIFI] Wifi disconnected"));
        m_wifiPrevState = false;
        disconnect();
        delay(2000);
        m_state->currentState = State::CONNECTING;
        Serial.println(F("[WIFI] Reconnecting"));
        connect(m_config->getWifiSsid(), m_config->getWifiPassword());
        
        m_wifiCheckTimedUl = millis() + 30000;
    }

    // restart MDNS if wifi is connected again
    if (WiFi.status() == WL_CONNECTED && m_wifiPrevState == false)
    {   
        m_wifiReconnectCount = 0;
        Serial.println(F("[WIFI] Wifi connected"));
        m_wifiPrevState = true;
        m_state->currentState = State::CONN_SUCCESS;
    }
}

void WifiService::connect(String ssid, String password)
{
    Serial.println(F("[WIFI] Connecting..."));
    WiFi.enableSTA(true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(m_config->getHostName().c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    m_state->currentState = State::CONNECTING;      
}

void WifiService::disconnect()
{
    WiFi.disconnect();
}