#include "service_ir.h"
#include <ArduinoJson.h>

InfraredService* InfraredService::s_instance = nullptr;

InfraredService::InfraredService()
{
    s_instance = this;
}

void InfraredService::init()
{
    irrecv.setUnknownThreshold(1000);
    irrecv.enableIRIn();
    irsend.begin();
}

void InfraredService::loop()
{
    if (receiving)
    {
        String code_received = receive();

        if (code_received != "")
        {
        StaticJsonDocument<500> responseDoc;
        responseDoc["type"] = "dock";
        responseDoc["command"] = "ir_receive";
        responseDoc["code"] = code_received;

        String message;
        serializeJson(responseDoc, message);
        messageToAPI = message;
        Serial.print(F("[IR] Sending message to API clients: "));
        Serial.println(message);
        }
    }
}

void InfraredService::doRestart(const char *str, const bool serial_only)
{
    delay(2000); // Enough time for messages to be sent.
    ESP.restart();
    delay(5000); // Enough time to ensure we don't return.
}

String InfraredService::receive()
{
    String code = "";

    if (irrecv.decode(&results)) {
        Serial.println(results.decode_type);
        Serial.println(results.address);
        Serial.println(results.bits);
        Serial.println(results.repeat);
        Serial.println(*resultToRawArray(&results));
        Serial.println(resultToHumanReadableBasic(&results));
        code = resultToHexidecimal(&results);
        Serial.println(code);
        int foo = getUInt64fromHex(code.c_str());
        Serial.println(foo);
        // uint16_t *raw = resultToRawArray(&results);
        // Serial.println(*raw);
        yield();
    }
    return code;
}

bool InfraredService::sendPronto(const String str, uint16_t repeats)
{
    Serial.println("[IR] Sending pronto codes.");
    return false;
}

bool InfraredService::send(const int decodeTypeInt, const String codeHex, const uint16_t bits, const uint16_t repeatCount)
{
    // Serial.println("[IR] Sending raw codes.");
    // decode_type_t decodeType = static_cast<decode_type_t>(decodeTypeInt);
    // Serial.println(codeHex);
    // uint64_t code = getUInt64fromHex(codeHex.c_str());
    // Serial.println((int)code);
    // return irsend.send(SONY, 0x240C OR 9228, 15, 5);

    // "4,0x640C,15,0"
    if (codeHex.indexOf(',') > 0) {
        const int firstIndex = codeHex.indexOf(',');
        const int secondIndex =  codeHex.indexOf(',', firstIndex + 1);
        const int thirdIndex = codeHex.indexOf(',', secondIndex + 1);

        decode_type_t dt = static_cast<decode_type_t>(codeHex.substring(0, firstIndex).toInt());
        uint64_t c = getUInt64fromHex(codeHex.substring(firstIndex + 1, secondIndex).c_str());
        uint16_t b = codeHex.substring(secondIndex + 1, thirdIndex).toInt();
        uint16_t r = codeHex.substring(thirdIndex + 1).toInt();

        Serial.println(dt);
        Serial.println((int)c);
        Serial.println(b);
        Serial.println(r);

        return irsend.send(dt, c, b, r);
    } else {
        decode_type_t decodeType = static_cast<decode_type_t>(decodeTypeInt);
        uint64_t code = getUInt64fromHex(codeHex.c_str());

        Serial.println(decodeType);
        Serial.println((int)code);
        Serial.println(bits);
        Serial.println(repeatCount);

        return irsend.send(decodeType, code, bits, repeatCount);
    }

    // return irsend.send(decodeType, code, bits, repeatCount);
}

String InfraredService::resultToHexidecimal(const decode_results * const result) {
  String output = F("0x");
  // Reserve some space for the string to reduce heap fragmentation.
  output.reserve(2 * kStateSizeMax + 2);  // Should cover worst cases.

  if (hasACState(result->decode_type)) {
    for (uint16_t i = 0; result->bits > i * 8; i++) {
      if (result->state[i] < 0x10) output += '0';  // Zero pad
      output += uint64ToString(result->state[i], 16);
    }
  } else {
    output += uint64ToString(result->value, 16);
  }
  return output;
}

// CONVERT HEX STRING TO INT!
uint64_t InfraredService::getUInt64fromHex(char const *str) {
  uint64_t result = 0;
  uint16_t offset = 0;
  // Skip any leading '0x' or '0X' prefix.
  if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) offset = 2;
  for (; isxdigit((unsigned char)str[offset]); offset++) {
    char c = str[offset];
    result *= 16;
    if (isdigit(c))
      result += c - '0';  // '0' .. '9'
    else if (isupper(c))
      result += c - 'A' + 10;  // 'A' .. 'F'
    else
      result += c - 'a' + 10;  // 'a' .. 'f'
  }
  return result;
}
