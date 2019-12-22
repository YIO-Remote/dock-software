#ifndef SERVICE_API_H
#define SERVICE_API_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <config.h>
#include <state.h>
#include <service_ir.h>
#include <led_control.h>

class API
{
public:
    explicit API();
    virtual ~API(){}

    static API*           getInstance() { return s_instance; }

    void                  init();
    void                  loop();
    void                  processData(String response, int id, String type);
    void                  sendMessage(String msg);

private:
    static API*           s_instance;
    // Config*               m_config = Config::getInstance();
    // State*                m_state = State::getInstance();
    // InfraredService*      m_ir = InfraredService::getInstance();
    // LedControl*           m_led = LedControl::getInstance();

    WebSocketsServer      m_webSocketServer = WebSocketsServer(946);
    uint8_t               m_webSocketClients[100] = {};
    int                   m_webSocketClientsCount = 0;

    void                  handleSerial();
};

#endif