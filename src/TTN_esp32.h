// TTN_esp32.h

#ifndef _TTN_ESP32_h
#define _TTN_ESP32_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"

#else
#include "WProgram.h"

#endif

#include "lorawan_esp32.h"
#include "nvs_flash.h"

#include "lmic/lmic/oslmic.h"

class TTN_esp32 : public Lorawan_esp32
{
public:
    ///
    /// Default constructor
    /// Silently tries to restore the keys
    ///
    TTN_esp32();

    ///
    /// Store the Device EUI, Application EUI, Application key,
    /// Device address, Network session key and Application session key
    /// in the non volatile memory
    ///
    bool saveKeys();

    ///
    /// Restore the Device EUI, Application EUI, Application key,
    /// Device address, Network session key and Application session key
    /// from the non volatile memory
    /// @param silent Set to false for debug output
    ///
    bool restoreKeys(bool silent = true);
    // bool decodeKeys(const char *dev_eui, const char *app_eui, const char
    // *app_key); bool fromMAC(const char *app_eui, const char *app_key);

    ///
    /// Copy the Application EUI into the given buffer
    /// @param buffer The buffer
    /// @param size The size of the buffer
    /// @return The number of bytes copied to the buffer
    ///
    size_t getAppEui(char* buffer, size_t size);

    ///
    /// Get the Application EUI as a String
    /// @return A string containing the Application EUI
    ///
    String getAppEui();

    ///
    /// Copy the Device EUI or MAC Address into the given buffer
    /// @param buffer The buffer
    /// @param size The size of the buffer
    /// @param hardwareEui Set true if you want to get the MAC Address of the ESP
    /// @return The number of bytes copied to the buffer
    ///
    size_t getDevEui(char* buffer, size_t size, bool hardwareEui = false);

    ///
    /// Get the Device EUI or MAC Address as a String
    /// @param hardwareEui Set true if you want to get the MAC Address of the ESP
    /// @return A string containing the Device EUI or MAC Address
    ///
    String getDevEui(bool hardwareEui = false);

    ///
    /// Provision the Application EUI and Application key
    /// @param appEui The Application EUI in MSB order
    /// @param appKey The Application key in MSB order
    /// @return True on success, false if not
    ///
    bool provision(const char* appEui, const char* appKey);

    ///
    /// Provision the Application EUI, Application key and Device EUI
    /// @param devEui The Device EUI in MSB order
    /// @param appEui The Application EUI in MSB order
    /// @param appKey The Application key in MSB order
    /// @return True on success, false if not
    ///
    bool provision(const char* devEui, const char* appEui, const char* appKey);

    ///
    /// Provision the Device address, Network key and Application key
    /// @param devAddr The Device address in MSB order
    /// @param nwkSKey The Network key in MSB order
    /// @param appSKey The Application Key in MSB order
    /// @return True on success, false if not
    ///
    bool provisionABP(const char* devAddr, const char* nwkSKey, const char* appSKey);

    ///
    /// Join the TTN
    ///
    /// Will check whether the keys are provided, if not try to load them from
    /// non volatile memory and if present join the network
    /// @return True if keys are present and join started, false if not
    ///
    bool join();

    ///
    /// Join the TTN
    ///
    /// Will provision the keys and join the network
    /// \note {
    ///		retries and retryDelay are not yet implemented
    /// }
    /// @param appEui The Application EUI in MSB order
    /// @param appKey The Application key in MSB order
    /// @param retries The number of retries until timeout
    /// @param retryDelay The delay in milliseconds between each retry
    ///
    bool join(const char* appEui, const char* appKey, int8_t retries = -1, uint32_t retryDelay = 10000);

    ///
    /// Join the TTN
    ///
    /// Will provision the keys and join the network
    /// \note {
    ///		retries and retryDelay are not yet implemented
    /// }
    /// @param devEui The Device EUI in MSB order
    /// @param appEui The Application EUI in MSB order
    /// @param appKey The Application key in MSB order
    /// @param retries The number of retries until timeout
    /// @param retryDelay The delay in milliseconds between each retry
    ///
    bool join(
        const char* devEui, const char* appEui, const char* appKey, int8_t retries = -1, uint32_t retryDelay = 10000);

    bool personalize();

    bool personalize(const char* devAddr, const char* nwkSKey, const char* appSKey);

    ///
    /// Show the current status
    ///
    /// Prints the current status to the serial console
    /// Included are Device EUI, Application EUI, netId, Device address, Network
    /// session key, Application session key, data rate, tx power and frequency
    ///
    void showStatus();
    // void static onMessage(void(*cb)(const uint8_t *payload, size_t size,
    // uint8_t port));

private:
    bool decode(bool incl_dev_eui, const char* devEui, const char* appEui, const char* appKey);
    bool readNvsValue(nvs_handle handle, const char* key, uint8_t* data, size_t expected_length, bool silent);
    bool writeNvsValue(nvs_handle handle, const char* key, const uint8_t* data, size_t len);
    void checkKeys();
    static bool hexStrToBin(const char* hex, uint8_t* buf, int len);
    static int hexTupleToByte(const char* hex);
    static int hexDigitToVal(char ch);
    static void binToHexStr(const uint8_t* buf, int len, char* hex);
    static char valToHexDigit(int val);
    static void swapBytes(uint8_t* buf, int len);
    static bool isAllZeros(const uint8_t* buf, int len);
};

#endif
