#include "service_ir.h"
#include <ArduinoJson.h>

InfraredService* InfraredService::s_instance = nullptr;

InfraredService::InfraredService()
{
    s_instance = this;
}

void InfraredService::init()
{
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

    if (irrecv.decode(&results))
    {
        for (uint16_t i = 1; i < results.rawlen; i++) {
            uint32_t usecs;
            for (usecs = results.rawbuf[i] * kRawTick; usecs > UINT16_MAX;
                usecs -= UINT16_MAX) {
            code += uint64ToString(UINT16_MAX);
            if (i % 2)
                code += F(", 0,  ");
            else
                code += F(",  0, ");
            }
            code += uint64ToString(usecs, 10);
            if (i < results.rawlen - 1)
            code += F(", ");            // ',' not needed on the last one
            if (i % 2 == 0) code += ' ';  // Extra if it was even.
        }
        yield(); // Feed the WDT (again)
        irrecv.resume();
    }
    return code;
}

// Parse a Pronto Hex String/code and send it.
// Args:
//   irsend: A ptr to the IRsend object to transmit via.
//   str: A comma-separated String of nr. of repeats, then hexadecimal numbers.
//        e.g. "R1,0000,0067,0000,0015,0060,0018,0018,0018,0030,0018,0030,0018,
//              0030,0018,0018,0018,0030,0018,0018,0018,0018,0018,0030,0018,
//              0018,0018,0030,0018,0030,0018,0030,0018,0018,0018,0018,0018,
//              0030,0018,0018,0018,0018,0018,0030,0018,0018,03f6"
//              or
//              "0000,0067,0000,0015,0060,0018". i.e. without the Repeat value
//        Requires at least kProntoMinLength comma-separated values.
//        sendPronto() only supports raw pronto code types, thus so does this.
//   repeats:  Nr. of times the message is to be repeated.
//             This value is ignored if an embeddd repeat is found in str.
// Returns:
//   bool: Successfully sent or not.
bool InfraredService::sendPronto(const String str, uint16_t repeats)
{
    uint16_t count;
    uint16_t *code_array;
    int16_t index = -1;
    uint16_t start_from = 0;

    // Find out how many items there are in the string.
    count = countValuesInStr(str, ',');

    // Check if we have the optional embedded repeats value in the code string.
    if (str.startsWith("R") || str.startsWith("r"))
    {
        // Grab the first value from the string, as it is the nr. of repeats.
        index = str.indexOf(',', start_from);
        repeats = str.substring(start_from + 1, index).toInt(); // Skip the 'R'.
        start_from = index + 1;
        count--; // We don't count the repeats value as part of the code array.
    }

    // We need at least kProntoMinLength values for the code part.
    if (count < kProntoMinLength)
        return false;

    // Now we know how many there are, allocate the memory to store them all.
    code_array = newCodeArray(count);

    // Rest of the string are values for the code array.
    // Now convert the hex strings to integers and place them in code_array.
    count = 0;
    do
    {
        index = str.indexOf(',', start_from);
        // Convert the hexadecimal value string to an unsigned integer.
        code_array[count] = strtoul(str.substring(start_from, index).c_str(),
                                    NULL, 16);
        start_from = index + 1;
        count++;
    } while (index != -1);

    irsend.sendPronto(code_array, count, repeats); // All done. Send it.
    free(code_array);                              // Free up the memory allocated.
    if (count > 0)
        return true; // We sent something.
    return false;    // We probably didn't.
}

// Parse an IRremote Raw Hex String/code and send it.
// Args:
//   irsend: A ptr to the IRsend object to transmit via.
//   str: A comma-separated String containing the freq and raw IR data.
//        e.g. "38000,9000,4500,600,1450,600,900,650,1500,..."
//        Requires at least two comma-separated values.
//        First value is the transmission frequency in Hz or kHz.
// Returns:
//   bool: Successfully sent or not.
bool InfraredService::send(const String str)
{
    Serial.println("[IR] Sending raw codes.");
    uint16_t count;
    uint16_t freq = 38000; // Default to 38kHz.
    uint16_t *raw_array;

    // Find out how many items there are in the string.
    count = countValuesInStr(str, ',');

    // We expect the frequency as the first comma separated value, so we need at
    // least two values. If not, bail out.
    if (count < 2)
        return false;
    count--; // We don't count the frequency value as part of the raw array.

    // Now we know how many there are, allocate the memory to store them all.
    raw_array = newCodeArray(count);

    // Grab the first value from the string, as it is the frequency.
    int16_t index = str.indexOf(',', 0);
    freq = str.substring(0, index).toInt();
    uint16_t start_from = index + 1;
    // Rest of the string are values for the raw array.
    // Now convert the strings to integers and place them in raw_array.
    count = 0;
    do
    {
        index = str.indexOf(',', start_from);
        raw_array[count] = str.substring(start_from, index).toInt();
        start_from = index + 1;
        count++;
    } while (index != -1);

    irsend.sendRaw(raw_array, count, freq); // All done. Send it.
    free(raw_array);                        // Free up the memory allocated.
    if (count > 0)
        return true; // We sent something.
    return false;    // We probably didn't.
}

// Count how many values are in the String.
// Args:
//   str:  String containing the values.
//   sep:  Character that separates the values.
// Returns:
//   The number of values found in the String.
uint16_t InfraredService::countValuesInStr(const String str, char sep)
{
    int16_t index = -1;
    uint16_t count = 1;
    do
    {
        index = str.indexOf(sep, index + 1);
        count++;
    } while (index != -1);
    return count;
}

// Dynamically allocate an array of uint16_t's.
// Args:
//   size:  Nr. of uint16_t's need to be in the new array.
// Returns:
//   A Ptr to the new array. Restarts the ESP if it fails.
uint16_t *InfraredService::newCodeArray(const uint16_t size)
{
    uint16_t *result;

    result = reinterpret_cast<uint16_t *>(malloc(size * sizeof(uint16_t)));
    // Check we malloc'ed successfully.
    if (result == NULL) // malloc failed, so give up.
        doRestart(
            "FATAL: Can't allocate memory for an array for a new message! "
            "Forcing a reboot!",
            true); // Send to serial only as we are in low mem
    return result;
}