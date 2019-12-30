#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <Arduino.h>

class LedControl
{
public:
    explicit LedControl();
    virtual ~LedControl(){}

    static LedControl*  getInstance() { return s_instance; }

    void setLedMaxBrightness(int value);
    int  getLedMaxBrightness() { return m_ledMaxBrightness; }

    const int       m_ledChannel = 0;

private:
    static LedControl*           s_instance;

    const int       m_ledGPIO = 23;
    const int       m_ledPWMFreq = 5000;
    const int       m_ledResolution = 8;

    int             m_ledMaxBrightness = 255;
    int             m_ledMapDelay = map(m_ledMaxBrightness, 5, 255, 30, 5);
    int             m_ledMapPause = map(m_ledMaxBrightness, 5, 255, 800, 0);

    TaskHandle_t    m_ledTask;

    static void    loopTask(void *pvParameter); 
    void           loop();     

};

#endif