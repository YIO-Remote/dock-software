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
        Serial.println(resultToHumanReadableBasic(&results));
        code += results.decode_type;
        code += ";";
        code += resultToHexidecimal(&results);
        code += ";";
        code += results.bits;
        code += ";";
        code += results.repeat;
        Serial.println(code);
        yield();
    }
    return code;
}

bool InfraredService::send(const String message, const String format)
{
    // Format is: "<protocol>,<hex-ir-code>,<bits>,<repeat-count>" e.g. "4,0x640C,15,0"
    const int firstIndex = message.indexOf(';');
    const int secondIndex =  message.indexOf(';', firstIndex + 1);
    const int thirdIndex = message.indexOf(';', secondIndex + 1);

    decode_type_t protocol = static_cast<decode_type_t>(message.substring(0, firstIndex).toInt());
    String commandStr = message.substring(firstIndex + 1, secondIndex).c_str();
    uint64_t command = getUInt64fromHex(commandStr.c_str());
    uint16_t bits = message.substring(secondIndex + 1, thirdIndex).toInt();
    uint16_t repeatCount = message.substring(thirdIndex + 1).toInt();

    if (format == "hex") {        
        return irsend.send(protocol, command, bits, repeatCount);
    } else {
        int16_t index = -1;
        uint16_t start_from = 0;
        uint16_t count = countValuesInStr(commandStr, ',');
        uint16_t *code_array = newCodeArray(count);

        count = 0;

        do {
            index = commandStr.indexOf(',', start_from);
            code_array[count] = strtoul(commandStr.substring(start_from, index).c_str(),  NULL, 16);
            start_from = index + 1;
            count++;
        } while (index != -1);

        irsend.sendPronto(code_array, count, repeatCount);
        free(code_array);
        return (count > 0);
    }

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

// CONVERT HEX STRING TO INT!
uint16_t * InfraredService::newCodeArray(const uint16_t size) {
  uint16_t *result;

  result = reinterpret_cast<uint16_t*>(malloc(size * sizeof(uint16_t)));
  // Check we malloc'ed successfully.
  if (result == NULL)  // malloc failed, so give up.
    doRestart(
        "FATAL: Can't allocate memory for an array for a new message! "
        "Forcing a reboot!", true);  // Send to serial only as we are in low mem
  return result;
}

uint16_t InfraredService::countValuesInStr(const String str, char sep) {
  int16_t index = -1;
  uint16_t count = 1;
  do {
    index = str.indexOf(sep, index + 1);
    count++;
  } while (index != -1);
  return count;
}
