#ifndef SERVICE_BLUETOOTH_H
#define SERVICE_BLUETOOTH_H

#include <Arduino.h>
#include <config.h>
#include <state.h>
#include <service_api.h>
#include "BluetoothSerial.h"

class BluetoothService
{
public:
    explicit BluetoothService();
    virtual ~BluetoothService() {}

    static BluetoothService* getInstance() { return s_instance; } 

    void init();
    void handle();

private:
    static BluetoothService* s_instance;

    BluetoothSerial*              m_bluetooth = new BluetoothSerial();
    State*                        m_state = State::getInstance();
    Config*                       m_config = Config::getInstance();
    API*                          m_api = API::getInstance();

    String                        m_receivedData = "";
    bool                          m_interestingData = false;
};

#endif