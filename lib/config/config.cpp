#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

Config* Config::s_instance = nullptr;

// initializing config
Config::Config()
{
    s_instance = this;

    // if no LED brightness setting, set default
    if (getLedBrightness() == 0)
    {
        Serial.println("[CONFIG] Setting default brightness");
        setLedBrightness(m_defaultLedBrightness);
    }

    // if no friendly name is set, set mac address
    if (getFriendlyName() == "")
    {
        // get the default friendly name
        Serial.println("[CONFIG] Setting default friendly name");
        setFriendlyName(getHostName());
    }
}

// getter and setter for brightness value
int Config::getLedBrightness()
{
    m_preferences.begin("general", false);
    int led_brightness = m_preferences.getInt("brightness", 0);

    if (led_brightness != 0)
    {
        Serial.print(F("[CONFIG] LED brightness setting found: "));
        Serial.println(led_brightness);
    }
    m_preferences.end();
    
    return led_brightness;
}

void Config::setLedBrightness(int value)
{
    m_preferences.begin("general", false);
    m_preferences.putInt("brightness", value);
    m_preferences.end();
}

// getter and setter for dock friendly name
String Config::getFriendlyName()
{
    m_preferences.begin("general", false);
    String friendlyName = m_preferences.getString("friendly_name", "");

    if (!friendlyName.equals(""))
    {
        Serial.print(F("[CONFIG] Friendly name was found: "));
        Serial.println(friendlyName);
    }
    m_preferences.end();

    return friendlyName;
}

void Config::setFriendlyName(String value)
{
    m_preferences.begin("general", false);
    m_preferences.putString("friendly_name", value);
    m_preferences.end();
}

// getter and setter for wifi credentials
String Config::getWifiSsid()
{
    m_preferences.begin("wifi", false);
    String ssid = m_preferences.getString("ssid", "");

    if (!ssid.equals(""))
    {
        Serial.print(F("[CONFIG] SSID was found: "));
        Serial.println(ssid);
    }
    m_preferences.end();

    return ssid;
}

void Config::setWifiSsid(String value)
{
    m_preferences.begin("wifi", false);
    m_preferences.putString("ssid", value);
    m_preferences.end();
}

String Config::getWifiPassword()
{
    m_preferences.begin("wifi", false);
    String password = m_preferences.getString("password", "");

    if (!password.equals(""))
    {
        Serial.print(F("[CONFIG] Wifi password was found: "));
        Serial.println(password);
    }
    m_preferences.end();

    return password;
}

void Config::setWifiPassword(String value)
{
    m_preferences.begin("wifi", false);
    m_preferences.putString("password", value);
    m_preferences.end();
}

// get hostname
String Config::getHostName()
{
    char dockHostName[] = "YIO-Dock-xxxxxxxxxxxx";
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    sprintf(dockHostName, "YIO-Dock-%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    return dockHostName;    
}

// reset config to defaults
void Config::reset()
{
    Serial.println("[CONFIG] Resetting configuration.");

    m_preferences.begin("general", false);
    m_preferences.clear();
    m_preferences.end();

    delay(500);

    m_preferences.begin("wifi", false);
    m_preferences.clear();
    m_preferences.end();

    int err;
    err = nvs_flash_init();
    Serial.println("[CONFIG] nvs_flash_init: " + err);
    err = nvs_flash_erase();
    Serial.println("[CONFIG] nvs_flash_erase: " + err);

    delay(500);

    Serial.println(F("[CONFIG] Resetting WiFi credentials."));
    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    if (esp_wifi_set_config(WIFI_IF_STA, &conf))
    {
      log_e("[CONFIG] clear config failed!");
    }

    delay(500);
    
    ESP.restart();
}