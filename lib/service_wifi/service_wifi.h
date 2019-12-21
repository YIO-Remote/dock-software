#ifndef SERVICE_WIFI_H
#define SERVICE_WIFI_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

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

    bool                          m_wifiPrevState = false; // previous WIFI connection state; 0 - disconnected, 1 - connected
    unsigned long                 m_wifiCheckTimedUl = 30000;

    WiFiManager*                  m_wifiManager;
    String                        m_ssid;
    String                        m_password;
};

#endif