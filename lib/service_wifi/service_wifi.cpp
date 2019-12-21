#include "service_wifi.h"
#include <config.h>
#include <state.h>
#include <service_mdns.h>

WifiService* WifiService::s_instance = nullptr;

WifiService::WifiService()
{
    s_instance = this;
}

void WifiService::initiateWifi()
{
    m_wifiManager->autoConnect(Config::getInstance()->getHostName().c_str());
    m_ssid = Config::getInstance()->getWifiSsid();
    m_password = Config::getInstance()->getWifiPassword();
    m_wifiPrevState = true;
    State::getInstance()->currentState = State::CONNECTING;
}

void WifiService::handleReconnect()
{
    if (WiFi.status() != WL_CONNECTED && (millis() > m_wifiCheckTimedUl))
    {
        m_wifiPrevState = false;
        MDNS.end();

        WiFi.disconnect();
        delay(1000);
        WiFi.enableSTA(true);
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.begin(m_ssid.c_str(), m_password.c_str());
        m_wifiCheckTimedUl = millis() + 30000;
    }

    // restart MDNS if wifi is connected again
    if (WiFi.status() == WL_CONNECTED && m_wifiPrevState == false)
    {
        m_wifiPrevState = true;
        MDNSService::getInstance()->init();
    }
}

void WifiService::connect(String ssid, String password)
{
    m_wifiManager->connectWifi(ssid, password);
}