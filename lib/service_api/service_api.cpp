#include "service_api.h"
#include "service_wifi.h"
#include "service_mdns.h"

API* API::s_instance = nullptr;

API::API()
{
    s_instance = this;
}

void API::init()
{
    // initialize the websocket server
    m_webSocketServer.begin();
    m_webSocketServer.onEvent([=](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
        switch (type)
        {
        case WStype_DISCONNECTED:
        {
            Serial.printf("[API] [%u] Disconnected!\n", num);
            // remove from connected clients
            for (int i = 0; i < m_webSocketClientsCount; i++)
            {
                if (m_webSocketClients[i] == num)
                {
                    m_webSocketClients[i] = -1;
                }
            }
        }
            break;

        case WStype_CONNECTED:
        {
            IPAddress ip = m_webSocketServer.remoteIP(num);
            Serial.printf("[API] [%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send auth request message
            StaticJsonDocument<200> responseDoc;
            responseDoc["type"] = "auth_required";
            String message;
            serializeJson(responseDoc, message);
            m_webSocketServer.sendTXT(num, message);
        }
            break;

        case WStype_TEXT:
        {
            processData(String((char *)payload), num, "websocket");
        }
            break;

        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        case WStype_BIN:
        case WStype_PING:
        case WStype_PONG:
            break;
        }
    });
}

void API::loop()
{
    m_webSocketServer.loop();
    handleSerial();
    if (InfraredService::getInstance()->messageToAPI != "")
    {
        sendMessage(InfraredService::getInstance()->messageToAPI);
        InfraredService::getInstance()->messageToAPI = "";
    }
}

void API::handleSerial()
{
    String receivedSerialData = "";
    bool interestingData = false; // Data between { and } chars.

    while (Serial.available() > 0)
    {
        char incomingChar = Serial.read();
        if (String(incomingChar) == "{")
        {
            interestingData = true;
            receivedSerialData = "";
        }
        else if (String(incomingChar) == "}")
        {
            interestingData = false;
            receivedSerialData += "}";
            // process the data
            processData(receivedSerialData, 0, "serial");
        }
        if (interestingData)
        {
            receivedSerialData += String(incomingChar);
        }
    }
}

void API::processData(String response, int id, String type)
{
    Serial.print("[API] GOT DATA FROM: ");
    Serial.println(type);
    Serial.println(response);

    StaticJsonDocument<600> webSocketJsonDocument;
    DeserializationError error = deserializeJson(webSocketJsonDocument, response);

    if (error)
    {
        Serial.print(F("[API] deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }

    // response json
    StaticJsonDocument<200> responseDoc;

    String type;
    if (webSocketJsonDocument.containsKey("type")) {
        type = webSocketJsonDocument["type"].as<String>();
    }

    String command;
    if (webSocketJsonDocument.containsKey("command")) {
        command = webSocketJsonDocument["command"].as<String>();
    }

    // NEW WIFI SETTINGS
    if (webSocketJsonDocument.containsKey("ssid") && webSocketJsonDocument.containsKey("password"))
    {
        String ssid = webSocketJsonDocument["ssid"].as<String>();
        String pass = webSocketJsonDocument["password"].as<String>();

        Config::getInstance()->setWifiSsid(ssid);
        Config::getInstance()->setWifiPassword(pass);

        Serial.println("[API] Saving SSID:" + ssid + " PASS:" + pass);

        Serial.println(F("[API] Disconnecting any current WiFi connections."));
        WiFi.disconnect();
        delay(1000);
        Serial.println(F("[API] Connecting to provided WiFi credentials."));
        WifiService::getInstance()->connect(ssid, pass);
    }
    
    // AUTHENTICATION TO THE API
    if (type == "auth")
    {
        if (webSocketJsonDocument.containsKey("token"))
        {
            if (webSocketJsonDocument["token"].as<String>() == Config::getInstance()->token)
            {
                // token ok
                responseDoc["type"] = "auth_ok";
                String message;
                serializeJson(responseDoc, message);

                if (type == "websocket")
                {
                    m_webSocketServer.sendTXT(id, message);

                    // add client to authorized clients
                    m_webSocketClients[m_webSocketClientsCount] = id;
                    m_webSocketClientsCount++;
                } else {
                    Serial.println(message);
                }
            }
            else
            {
                // invalid token
                responseDoc["type"] = "auth";
                responseDoc["message"] = "Invalid token";
                String message;
                serializeJson(responseDoc, message);
                if (type == "websocket")
                {
                    m_webSocketServer.sendTXT(id, message);
                } else {
                    Serial.println(message); 
                }
            }
        }
        else
        {
            // token needed
            responseDoc["type"] = "auth";
            responseDoc["message"] = "Token needed";
            String message;
            serializeJson(responseDoc, message);
            if (type == "websocket")
            {
                m_webSocketServer.sendTXT(id, message);
            } else {
                Serial.println(message); 
            }
        }
    }

    // COMMANDS TO THE DOCK
    for (int i = 0; i < m_webSocketClientsCount; i++)
    {
        if (m_webSocketClients[i] == id)
        {
            // it's on the list, let's see what it wants
            if (type == "dock")
            {
                // Ping pong
                if (command == "ping")
                {
                    Serial.println(F("[API] Sending heartbeat"));
                    responseDoc["type"] = "dock";
                    responseDoc["message"] = "pong";
                    String message;
                    serializeJson(responseDoc, message);
                    m_webSocketServer.sendTXT(id, message);
                }

                // Change LED brightness
                if (command == "led_brightness_start")
                {
                    State::getInstance()->currentState = State::LED_SETUP;
                    int maxbrightness = webSocketJsonDocument["brightness"].as<int>();
                    LedControl::getInstance()->setLedMaxBrightness(maxbrightness);

                    Serial.println(F("[API] Led brightness start"));
                    Serial.print(F("Brightness: "));
                    Serial.println(maxbrightness);
                }
                if (command == "led_brightness_stop")
                {
                    State::getInstance()->currentState = State::NORMAL;
                    ledcWrite(LedControl::getInstance()->m_ledChannel, 0);

                    Serial.println(F("[API] Led brightness stop"));

                    // save settings
                    Config::getInstance()->setLedBrightness(LedControl::getInstance()->getLedMaxBrightness());
                }

                // Send IR code
                if (command == "ir_send")
                {
                    responseDoc["type"] = "dock";
                    responseDoc["message"] = "ir_send";

                    Serial.println(F("[API] IR Send"));
                    if (webSocketJsonDocument["format"] == "pronto")
                    {
                        bool result = InfraredService::getInstance()->sendPronto(webSocketJsonDocument["code"].as<String>(), 1);
                        responseDoc["success"] = result;
                    }
                    else if (webSocketJsonDocument["format"] == "hex")
                    {
                        String code = webSocketJsonDocument["code"].as<String>();
                        uint16_t decodeType = webSocketJsonDocument["decodeType"].as<uint16_t>();
                        uint16_t bits = webSocketJsonDocument["bits"].as<uint16_t>();
                        uint16_t repeatCount = webSocketJsonDocument["repeat"].as<uint16_t>();
                        bool result = InfraredService::getInstance()->send(decodeType, code, bits, repeatCount);
                        responseDoc["success"] = result;
                    }
                    
                    String message;
                    serializeJson(responseDoc, message);
                    if (type == "websocket")
                    {
                        m_webSocketServer.sendTXT(id, message);
                    } else {
                        Serial.println(message); 
                    }
                }

                // Turn on IR receiving
                if (command == "ir_receive_on")
                {
                    InfraredService::getInstance()->receiving = true;
                    Serial.println(F("[API] IR Receive on"));
                }

                // Turn off IR receiving
                if (command == "ir_receive_off")
                {
                    InfraredService::getInstance()->receiving = false;
                    Serial.println(F("[API] IR Receive off"));
                }

                // Change state to indicate remote is fully charged
                if (command == "remote_charged")
                {
                    State::getInstance()->currentState = State::NORMAL_FULLYCHARGED;
                }

                // Change state to indicate remote is low battery
                if (command == "remote_lowbattery")
                {
                    State::getInstance()->currentState = State::NORMAL_LOWBATTERY;
                }

                // Change friendly name
                if (command == "set_friendly_name")
                {
                    String dockFriendlyName = webSocketJsonDocument["friendly_name"].as<String>();
                    Config::getInstance()->setFriendlyName(dockFriendlyName);
                    MDNSService::getInstance()->addFriendlyName(dockFriendlyName);     
                }

                // Reboot the dock
                if (command == "reboot")
                {
                    Serial.println(F("[API] Rebooting"));
                    State::getInstance()->reboot();
                }

                // Erase and reset the dock
                if (command == "reset")
                {
                    Serial.println(F("[API] Reset"));
                    Config::getInstance()->reset();
                }
            }
        }
    }
}

void API::sendMessage(String msg)
{
    for (int i = 0; i < m_webSocketClientsCount; i++)
    {
        m_webSocketServer.sendTXT(m_webSocketClients[i], msg);
    }
}