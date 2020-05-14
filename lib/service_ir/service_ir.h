#ifndef SERVICE_IR_H
#define SERVICE_IR_H

#include <Arduino.h>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRremoteESP8266.h" // https://platformio.org/lib/show/1089/IRremoteESP8266
#include <IRac.h>
#include <IRutils.h>
#include <IRtimer.h>

class InfraredService
{
public:
    explicit InfraredService();
    virtual ~InfraredService(){}

    static InfraredService*     getInstance() { return s_instance; }

    void                        init();
    void                        loop();
    void                        doRestart(const char *str, const bool serial_only);

    String                      receive();

    bool                        sendPronto(const String str, uint16_t repeats); // pronto
    bool                        send(const int decodeTypeInt, const String codeHex, const uint16_t bits, const uint16_t repeatCount);

    decode_results              results;
    bool                        receiving = false;
    String                      messageToAPI = "";

private:
    static InfraredService*     s_instance;

    const uint16_t              kRecvPin = 22;
    const uint16_t              kIrLedPin = 19;
    const uint32_t              kBaudRate = 115200;
    const uint16_t              kCaptureBufferSize = 1024; // 1024 == ~511 bits
    const uint8_t               kTimeout = 15;              // Milli-Seconds
    const uint16_t              kFrequency = 38000;        // in Hz. e.g. 38kHz.
    const uint16_t              kMinUnknownSize = 12;
    String resultToHexidecimal(const decode_results * const result);
    uint64_t getUInt64fromHex(char const *str);

    IRsend                      irsend = IRsend(kIrLedPin);
    IRrecv                      irrecv = IRrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
};

#endif