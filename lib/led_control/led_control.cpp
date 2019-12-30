#include "led_control.h"
#include "state.h"

LedControl* LedControl::s_instance = nullptr;

LedControl::LedControl()
{
  s_instance = this;
  
  xTaskCreatePinnedToCore(&LedControl::loopTask, "LedTask", 10000, this, 1, &m_ledTask, 0);
}

void LedControl::loopTask(void *pvParameter)
{
	LedControl* led = reinterpret_cast<LedControl*>(pvParameter);
	led->loop();
}

void LedControl::loop()
{
    // LED setup
  pinMode(m_ledGPIO, OUTPUT);

  ledcSetup(m_ledChannel, m_ledPWMFreq, m_ledResolution);
  ledcAttachPin(m_ledGPIO, m_ledChannel);

  while (1)
  {
    vTaskDelay(2 / portTICK_PERIOD_MS);

    // if the remote is charging, pulsate the LED
    // normal operation, LED off, turns on when charging
    if (State::getInstance()->currentState == State::NORMAL_CHARGING)
    {
      m_ledMapDelay = map(m_ledMaxBrightness, 5, 255, 30, 5);
      m_ledMapPause = map(m_ledMaxBrightness, 5, 255, 800, 0);

      for (int dutyCycle = 0; dutyCycle <= m_ledMaxBrightness; dutyCycle++)
      {
        // changing the LED brightness with PWM
        ledcWrite(m_ledChannel, dutyCycle);
        delay(m_ledMapDelay);
      }

      delay(m_ledMapPause);

      // decrease the LED brightness
      for (int dutyCycle = m_ledMaxBrightness; dutyCycle >= 0; dutyCycle--)
      {
        // changing the LED brightness with PWM
        ledcWrite(m_ledChannel, dutyCycle);
        delay(m_ledMapDelay);
      }
      delay(1000);
    }
    // needs setup
    else if (State::getInstance()->currentState == State::SETUP)
    {
      ledcWrite(m_ledChannel, 255);
      delay(800);
      ledcWrite(m_ledChannel, 0);
      delay(800);
    }
    // connecting to wifi, turning on OTA
    else if (State::getInstance()->currentState == State::CONNECTING)
    {
      ledcWrite(m_ledChannel, 255);
      delay(300);
      ledcWrite(m_ledChannel, 0);
      delay(300);
    }
    // successful connection
    else if (State::getInstance()->currentState == State::CONNECTING)
    {
      // Blink the LED 3 times to indicate successful connection
      for (int i = 0; i < 4; i++)
      {
        ledcWrite(m_ledChannel, 255);
        delay(100);
        ledcWrite(m_ledChannel, 0);
        delay(100);
      }
      State::getInstance()->currentState = State::NORMAL;
    }
    // LED brightness setup
    else if (State::getInstance()->currentState == State::LED_SETUP)
    {
      ledcWrite(m_ledChannel, m_ledMaxBrightness);
      delay(100);
    }
    // normal operation, remote fully charged
    else if (State::getInstance()->currentState == State::NORMAL_FULLYCHARGED)
    {
      ledcWrite(m_ledChannel, m_ledMaxBrightness);
      delay(100);
    }
    // normal operation, blinks to indicate remote is low battery
    else if (State::getInstance()->currentState == State::NORMAL_LOWBATTERY)
    {
      for (int i = 0; i < 2; i++)
      {
        ledcWrite(m_ledChannel, m_ledMaxBrightness);
        delay(100);
        ledcWrite(m_ledChannel, 0);
        delay(100);
      }
      delay(4000);
    }
  }
}

void LedControl::setLedMaxBrightness(int value)
{
    m_ledMaxBrightness = value;
}