// TTN_esp32.h

#ifndef _TTN_ESP32_h
#define _TTN_ESP32_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"

#else
#include "WProgram.h"

#endif

#include "lmic/arduino_lmic_hal_boards.h"
#include "lmic/lmic.h"
#include "lmic/lmic/oslmic.h"

//#define DEBUG

class TTN_esp32
{
public:
    ///
    /// Default constructor
    /// Silently tries to restore the keys
    ///
    TTN_esp32();
    ///
    /// Get the instance of TTN_esp32
    ///
    /// @return The singleton instance of TTN_esp32
    ///
    static TTN_esp32* getInstance();

    ///
    /// Disallow copying
    ///
    TTN_esp32(const TTN_esp32& ref) = delete;

    ///
    /// Start the LMIC stack with Pre-integrated boards
    ///
    /// @return True if the radio has been initialised, false if not
    ///
    bool begin();

    ///
    /// Initialize the LMIC stack with pinout as arguments
    ///
    /// @param nss The pin for chip select
    /// @param rxtx The pin for rx/tx control
    /// @param rst The pin for reset
    /// @param dio0 The DIO0 pin
    /// @param dio1 The DOI1 pin
    /// @param dio2 The DIO2 pin
    /// @return True if the radio has been initialised, false if not
    ///
    bool begin(uint8_t nss, uint8_t rxtx, uint8_t rst, uint8_t dio0, uint8_t dio1, uint8_t dio2);

    ///
    /// Initialize the stack with pointer to pin mapping
    ///
    /// @param pPinmap A pointer to the HalPinmap_t object
    /// @return True if the radio has been initialised, false if not
    ///
    bool begin(const TTN_esp32_LMIC::HalPinmap_t* pPinmap);

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
    /// @param nwkSKey The Network session key in MSB order
    /// @param appSKey The Application session key in MSB order
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
    /// @param force Force the provisioning
    /// @param retries The number of retries until timeout
    /// @param retryDelay The delay in milliseconds between each retry
    ///
    bool join(
        const char* appEui, const char* appKey, bool force = true, int8_t retries = -1, uint32_t retryDelay = 10000);

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
    /// @param force Force the provisioning
    /// @param retries The number of retries until timeout
    /// @param retryDelay The delay in milliseconds between each retry
    ///
    bool join(const char* devEui, const char* appEui, const char* appKey, bool force = true, int8_t retries = -1,
        uint32_t retryDelay = 10000);

    ///
    /// Activate the device via ABP.
    ///
    /// @return True if the activation was successful, false if not
    ///
    bool personalize();

    ///
    /// Activate the device via ABP.
    ///
    /// @param devAddr Device address assigned to the device
    /// @param nwkSKey Network session key assigned to the device for identification
    /// @param appSKey Application session key assigned to the device for encryption
    /// @return True if the activation was successful, false if not
    ///
    bool personalize(const char* devAddr, const char* nwkSKey, const char* appSKey);

    ///
    /// Send a message to the application using raw bytes.
    ///
    /// @param payload The payload to send
    /// @param length The size of the payload, use sizeof(payload)
    /// @param port The optional port to address, default 1
    /// @param confirm Whether to ask for confirmation, default 0(false)
    ///
    bool sendBytes(uint8_t* payload, size_t length, uint8_t port = 1, uint8_t confirm = 0);

    ///
    /// Send a message to the application at an interval using raw bytes.
    ///
    /// \note {Currently not implemented, only sends once}
    ///
    /// @param payload The payload to send
    /// @param length The size of the payload, use sizeof(payload)
    /// @param interval The interval in seconds to send at
    /// @param port The optional port to address, default 1
    /// @param confirm Whether to ask for confirmation, default 0(false)
    ///
    void sendBytesAtInterval(
        uint8_t* payload, size_t length, uint32_t interval = 60, uint8_t port = 1, uint8_t confirm = 0);

    ///
    /// Poll for incoming messages
    ///
    /// Calls sendBytes() with { 0x00 } as payload.
    ///
    /// @param port The port to address, default 1
    /// @param confirm Whether to ask for confirmation, default 0(false)
    /// @return True on success, false if not
    ///
    bool poll(uint8_t port = 1, uint8_t confirm = 0);

    ///
    /// Stops the TTN task, which runs the stack
    ///
    /// @return True if task was stopped
    ///
    bool stop(void);
    bool isRunning(void);
    ///
    /// Sets a function which will be called to process incoming messages
    ///
    /// You'll want to do this in your setup() function and then define a void (*callback)(const byte* payload, size_t
    /// length, port_t port) function somewhere else in your sketch.
    ///
    /// @param callback The callback which gets called
    ///
    void onMessage(void (*callback)(const uint8_t* payload, size_t size, int rssi));

    ///
    /// Sets a function which will be called upon LMIC events occur
    ///
    /// It will get called after the internal handling of the event
    ///
    /// @param callback The callback which gets called
    ///
    void onEvent(void (*callback)(const ev_t event));

    ///
    /// Check whether we have joined TTN
    /// @return True when joined, false if not
    ///
    bool isJoined();

    ///
    /// Check whether LMIC stack is trasnmitting or receiving
    ///
    /// @return True if so, false if not
    ///
    bool isTransceiving();

    ///
    /// Wait until all pending transactions have been handled
    ///
    /// Busy wait using delay
    ///
    /// @return The number of milliseconds we have waited
    ///
    uint32_t waitForPendingTransactions();

    ///
    /// Check whether the Device address, Network session key and Application session key
    /// are present
    ///
    bool hasSession();

    //
    /// Store the current session to the NVM
    ///
    /// Stores the device address, network session key and application router session key
    /// @param deviceAddress The device address
    /// @param networkSessionKey The network session key
    /// @param applicationRouterSessionKey The application router session key
    /// @return True if successfully stored, false if not
    ///
    bool storeSession(devaddr_t deviceAddress, u1_t networkSessionKey[16], u1_t applicationRouterSessionKey[16]);

    //
    /// Get the current session from the NVM
    ///
    /// Gets the device address, network session key and application router session key
    /// @param deviceAddress The device address
    /// @param networkSessionKey The network session key
    /// @param applicationRouterSessionKey The application router session key
    /// @return True if successfully loaded, false if not
    ///
    bool getSession(char* deviceAddress, char* networkSessionKey, char* applicationRouterSessionKey);

    ///
    /// Deletes the current session from NVM
    ///
    /// Removes the device address, network session key and application session key from NVM
    ///
    void deleteSession();

    ///
    /// Check whether the Device EUI, Application EUI and Application key
    /// are present
    ///
    /// @return True if provisioned, false if not
    ///
    bool isProvisioned();

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
    ///
    /// Erase the Device EUI, Application EUI, Application key,
    /// Device address, Network session key and Application session key
    /// in the non volatile memory. All the bytes are 00.
    ///
    bool eraseKeys();
    //
    /// Store the current frame counter in NVM
    ///
    bool storeFrameCounter();

    //
    /// Get the current frame counter from NVM
    ///
    uint32_t getFrameCounter();

    ///
    /// Show the current status
    ///
    /// Prints the current status to the serial console
    /// Included are Device EUI, Application EUI, netId, Device address, Network
    /// session key, Application session key, data rate, tx power and frequency
    ///
    void showStatus();

    ///
    /// Get the currently used data rate
    ///
    /// @return The data rate
    uint8_t getDatarate();

    ///
    /// Set the data rate of the radio
    ///
    /// @return True if data rate was set, false if not
    ///
    bool setDataRate(uint8_t rate = 7);

    ///
    /// Set the interval at which to send cyclic data
    ///
    /// \note {You must call \ref sendBytesAtInterval first, otherwise this will have no effect}
    ///
    /// @param interval The interval to set
    ///
    void setTXInterval(const uint32_t interval);

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
    /// Get the MAC address of the ESP
    ///
    /// @return The MAC address in Colon-Hexadecimal notation
    ///
    static String getMac();

    ///
    /// Get the currently used port that is addressed
    ///
    /// @return The port
    ///
    uint8_t getPort();

    ///
    /// Get the currently used frequency in Hz
    ///
    /// @return The frequency
    uint32_t getFrequency();

    ///
    /// Get the currently used transmit power in dB
    ///
    /// @return The transmit power
    int8_t getTXPower();

    bool setDevEui(byte* value);
    bool setAppEui(byte* value);
    bool setAppKey(byte* value);
    size_t getDevEui(byte* buf);
    size_t getAppEui(byte* buf);
    size_t getAppKey(byte* buf);

private:

    bool txBytes(uint8_t* payload, size_t length, uint8_t port, uint8_t confirm);
    void txMessage(osjob_t* job);
    void personalize(u4_t netID, u4_t DevAddr, uint8_t* NwkSKey, uint8_t* AppSKey);
    bool decode(bool includeDevEui, const char* devEui, const char* appEui, const char* appKey);
    void checkKeys();
    static void loopStack(void* parameter);
    void (*messageCallback)(const uint8_t* payload, size_t size, int rssi) = NULL;
    void (*eventCallback)(const ev_t event) = NULL;

    ///
    /// LMIC callback for getting the application key
    ///
    /// This is a friend so the function can access the private variables of this class so it can be declared globally
    ///
    friend void os_getDevKey(xref2u1_t buf);
    ///
    /// LMIC callback for getting the application EUI
    ///
    /// This is a friend so the function can access the private variables of this class so it can be declared globally
    ///
    friend void os_getArtEui(xref2u1_t buf);
    ///
    /// LMIC callback for getting the device EUI
    ///
    /// This is a friend so the function can access the private variables of this class so it can be declared globally
    ///
    friend void os_getDevEui(xref2u1_t buf);
    ///
    /// LMIC callback for events
    ///
    /// This is a friend so the function can access the private variables of this class so it can be declared globally
    ///
    friend void onEvent(ev_t ev);

protected:
    uint8_t dev_eui[8];
    uint8_t app_eui[8];
    uint8_t app_key[16];
    bool joined;

private:
    bool provisioned;
    bool session;
    TaskHandle_t TTN_task_Handle;
    uint8_t* _message;
    uint8_t _length;
    uint8_t _port;
    uint8_t _confirm;
    uint32_t txInterval;
    bool cyclique;
    osjob_t sendjob;

private:
    static TTN_esp32* _instance;
};

#endif
