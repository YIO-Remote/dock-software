#include "service_blueooth.h"

BluetoothService* BluetoothService::s_instance = nullptr;

BluetoothService::BluetoothService()
{
    s_instance = this;
}

void BluetoothService::init()
{
  m_bluetooth->register_callback([=](esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
    if(event == ESP_SPP_SRV_OPEN_EVT){
      Serial.println(F("[BLUETOOTH] Client Connected"));
    }

    if(event == ESP_SPP_CLOSE_EVT ){
      Serial.println(F("[BLUETOOTH] Client disconnected"));
    }
  });

  if (m_bluetooth->begin(m_config->getHostName())) {
      Serial.println(F("[BLUETOOTH] Initialized. Ready for setup."));
  } else {
    Serial.println(F("[BLUETOOTH] Failed to initialize."));
  }
}

void BluetoothService::handle()
{
    char incomingChar = m_bluetooth->read();
    int charAsciiNumber = incomingChar + 0;
    if (charAsciiNumber != 255) //ASCII 255 is continually send
    {
      m_bluetooth->print(incomingChar); //Echo send characters.
    }

    if (String(incomingChar) == "{")
    {
      m_interestingData = true;
      m_receivedData = "";
    }
    else if (String(incomingChar) == "}")
    {
      m_interestingData = false;
      m_receivedData += "}";
      m_api->processData(m_receivedData, 0, "bluetooth");
      m_receivedData = "";
    }
    if (m_interestingData)
    {
      m_receivedData += String(incomingChar);
    }
    delay(10);
}