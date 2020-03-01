#include <TTN_esp32.h>

#include "TTN_CayenneLPP.h"
/***************************************************************************
 *  Go to your TTN console register a device then the copy fields
 *  and replace the CHANGE_ME strings below
 ****************************************************************************/
const char* devEui = "CHANGE_ME"; // Change to TTN Device EUI
const char* appEui = "CHANGE_ME"; // Change to TTN Application EUI
const char* appKey = "CHANGE_ME"; // Chaneg to TTN Application Key

TTN_esp32 ttn ;
TTN_CayenneLPP lpp;

void message(const uint8_t* payload, size_t size, int rssi)
{
    Serial.println("-- MESSAGE");
    Serial.print("Received " + String(size) + " bytes RSSI=" + String(rssi) + "db");
    for (int i = 0; i < size; i++)
    {
        Serial.print(" " + String(payload[i]));
        // Serial.write(payload[i]);
    }

    Serial.println();
}

void print_wakeup_reason()
{
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
    case 1:
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case 2:
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        break;
    case 3:
        Serial.println("Wakeup caused by timer");
        break;
    case 4:
        Serial.println("Wakeup caused by touchpad");
        break;
    case 5:
        Serial.println("Wakeup caused by ULP program");
        break;
    default:
        Serial.println("Wakeup was not caused by deep sleep");
        break;
    }
}

void waitForTransactions()
{
    Serial.println("Waiting for pending transactions... ");
    Serial.println("Waiting took " + String(ttn.waitForPendingTransactions()) + "ms");
}

void sendData()
{
    const float temperature = temperatureRead();
    lpp.reset();
    lpp.addTemperature(1, temperature);
    if (ttn.sendBytes(lpp.getBuffer(), lpp.getSize()))
    {
        Serial.printf("Temp: %f TTN_CayenneLPP: %d %x %02X%02X\n", temperature, lpp.getBuffer()[0], lpp.getBuffer()[1],
            lpp.getBuffer()[2], lpp.getBuffer()[3]);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting");

    // Print the wakeup reason for ESP32
    print_wakeup_reason();

    ttn.begin();
    // Declare callback function for handling downlink messages from server
    ttn.onMessage(message);
    // Join the network
    ttn.join(devEui, appEui, appKey);
    Serial.print("Joining TTN ");
    while (!ttn.isJoined())
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\njoined!");

    // Make sure any pending transactions are handled first
    waitForTransactions();
    // Send our data
    sendData();
    // Make sure our transactions is handled before going to sleep
    waitForTransactions();

    // Configure GPIO33 as ext0 wake up source for HIGH logic level
    // esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 1);
    // OR
    // Set timer to 30 seconds
    // Sleep time in micro seconds so multiply by 1000000
    esp_sleep_enable_timer_wakeup(30 * 1000000);

    // Go to sleep now
    Serial.println("Going to sleep!");
    esp_deep_sleep_start();
    // Everything beyond this point will never be called
}

void loop()
{
    // Never called
}