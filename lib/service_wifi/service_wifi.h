#ifndef SERVICE_WIFI_H
#define SERVICE_WIFI_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <config.h>
#include <state.h>
#include <service_mdns.h>

class WifiService
{
public:
    explicit WifiService();
    virtual ~WifiService() {}

    static WifiService*           getInstance() { return s_instance; }

    void initiateWifi();
    void handleReconnect();
    void connect(String ssid, String password);

private:
    static WifiService*           s_instance;

    State*                        m_state = State::getInstance();
    Config*                       m_config = Config::getInstance();
    MDNSService*                  m_mdns = MDNSService::getInstance();              

    bool                          m_wifiPrevState = false; // previous WIFI connection state; 0 - disconnected, 1 - connected
    unsigned long                 m_wifiCheckTimedUl = 30000;

    WiFiManager                   m_wifiManager;
    String                        m_ssid;
    String                        m_password;
};

#endif