#include "service_wifi.h"

WifiService* WifiService::s_instance = nullptr;

WifiService::WifiService()
{
    s_instance = this;
}

void WifiService::initiateWifi()
{
    m_state->currentState = State::CONNECTING;
    m_wifiManager.autoConnect(m_config->getHostName().c_str());
    m_ssid = m_config->getWifiSsid();
    m_password = m_config->getWifiPassword();
    m_wifiPrevState = true;
    m_state->currentState = State::CONN_SUCCESS;
}

void WifiService::handleReconnect()
{
    if (WiFi.status() != WL_CONNECTED && (millis() > m_wifiCheckTimedUl))
    {
        Serial.println(F("[WIFI] Wifi disconnected"));
        m_wifiPrevState = false;
        MDNS.end();
        m_mdns->running = false;

        WiFi.disconnect();
        delay(1000);
        Serial.println(F("[WIFI] Reconnecting"));
        WiFi.enableSTA(true);
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.begin(m_ssid.c_str(), m_password.c_str());
        m_wifiCheckTimedUl = millis() + 30000;
    }

    // restart MDNS if wifi is connected again
    if (WiFi.status() == WL_CONNECTED && m_wifiPrevState == false)
    {   
        Serial.println(F("[WIFI] Wifi connected"));
        m_wifiPrevState = true;
        m_mdns->init();
        m_mdns->running = true;
    }
}

void WifiService::connect(String ssid, String password)
{
    m_wifiManager.connectWifi(ssid, password);
}