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
        m_wifiReconnectCount++;

        if (m_wifiReconnectCount == 5) {
            m_state->reboot();
        }

        Serial.println(F("[WIFI] Wifi disconnected"));
        m_wifiPrevState = false;
        m_mdns->running = false;
        MDNS.end();

        WiFi.disconnect();
        delay(2000);
        m_state->currentState = State::CONNECTING;
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
        m_wifiReconnectCount = 0;
        Serial.println(F("[WIFI] Wifi connected"));
        m_wifiPrevState = true;
        // m_mdns->init();
        m_mdns->running = true;
        m_state->currentState = State::CONN_SUCCESS;
    }
}

void WifiService::connect(String ssid, String password)
{
    m_wifiManager.connectWifi(ssid, password);
}