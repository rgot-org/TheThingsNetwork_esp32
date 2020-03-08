# API Reference

The `TTN_esp32` class enables ESP32 devices with supported LoRaWAN modules to communicate via The Things Network. This library is a wrapper for MCCI-Catena/arduino-lmic library.  the pre-integrated boards are the same as mcci-catena library plus :
 
 - HELTEC Wifi Lora 32 (V1 & V2)
 - HELTEC Wireless Stick
# Configuration
Like MCCI-Catena/arduino-lmic library the configuration is setup by editing `project_config/lmic_project_config.h`. See https://github.com/mcci-catena/arduino-lmic#configuration for more informations

## Class: `TTN_esp32`

Include and instantiate the TTN_esp32 class. The constructor initialize the library with the Streams it should communicate with. 

```c
#include <TTN_esp32.h>

TTN_esp32 ttn;
```
## Method: `begin`

Start the LMIC stack with Pre-integrated boards (see https://github.com/mcci-catena/arduino-lmic#pre-integrated-boards) 

Following boards are tested :
 - HELTEC WIRELESS STICK
 - HELTEC WIFI LORA 32 V1
 - HELTEC WIFI LORA 32 V2

 
```c
void begin();
```
Initialize the stack with pointer to pin mapping (see https://github.com/mcci-catena/arduino-lmic#pin-mapping)
```c
bool begin(const TTN_esp32_LMIC::HalPinmap_t* pPinmap);
```
Initialize the LMIC stack with pinout as arguments
```c
void begin(uint8_t nss, uint8_t rxtx, uint8_t rst, uint8_t dio0, uint8_t dio1, uint8_t dio2);
```
Example for HELTEC WIRELESS STICK
```c
// wireless stick pinout
#define UNUSED_PIN 0xFF
#define SS 18
#define RST_LoRa 14
#define DIO0 26
#define DIO1 35
#define DIO2 34
...
ttn.begin(SS, UNUSED_PIN, RST_LoRa, DIO0,DIO1,DIO2);
```
## Method: `getAppEui`

Gets the provisioned AppEUI. The AppEUI is set using `provision()` or `join()`.

```c
size_t getAppEui(char *buffer, size_t size);
```
return AppEui as an array of char
```c
String getAppEui();
```
return AppEui as String
## Method: `getDevEui`
Gets the provisioned DevEUI. The DevEUI is set using `provision()` or `join()`.
```c
size_t getDevEui(char *buffer, size_t size, bool hardwareEUI=false);
```
return DevEUI as array of char
```c
String getDevEui(bool hardwareEui=false);
```
return DevEUI as String

- `bool hardwareEui=false`: if true get DevEUI from Mac Address.

## Method: `isJoined`
Check whether we have joined TTN
```c
 bool isJoined();
```
return `true` if joined to TTN, `false` if not.
## Method: `showStatus`

Writes information about the device and LoRa module to `Serial` .

```c
void showStatus();
```

Will write something like:

```bash
---------------Status--------------
Device EUI: 004D22F44E5DCE53
Application EUI: 70B3D57ED0015575
netid: 13
devaddr: 26011B24
NwkSKey: 6A2D3C24AD3C0D17614D7325BCAA976
AppSKey: 9E68DCBEBF8AE9D891252FB7E6054
data rate: 5
tx power: 14dB
freq: 867100000Hz
-----------------------------------
```

## Method: `onMessage`

Sets a function which will be called to process incoming messages. You'll want to do this in your `setup()` function and then define a `void (*cb)(const byte* payload, size_t length, port_t port)` function somewhere else in your sketch.

```c
void onMessage(void (*cb)(const uint8_t *payload, size_t size, int rssi));
```

- `const uint8_t* payload`: Bytes received.
- `size_t size`: Number of bytes.
- `int rssi`: the rssi in dB.

See the [ttn-otaa](https://github.com/rgot-org/TheThingsNetwork_esp32/blob/master/examples/ttn-otaa/ttn-otaa.ino) example.

## Method: `join`

Activate the device via OTAA (default).

```c
bool join();
bool join(const char *app_eui, const char *app_key, int8_t retries = -1, uint32_t retryDelay = 10000);
bool join(const char *dev_eui, const char *app_eui, const char *app_key, int8_t retries = -1, uint32_t retryDelay = 10000);
```

- `const char *app_eui`: Application EUI the device is registered to.
- `const char *app_key`: Application Key assigned to the device.
- `const char *dev_eui`: Device EUI 
- `int8_t retries = -1`: Number of times to retry after failed or unconfirmed join. Defaults to `-1` which means infinite.
- `uint32_t retryDelay = 10000`: Delay in ms between attempts. Defaults to 10 seconds.

Returns `true` or `false` depending on whether it received confirmation that the activation was successful before the maximum number of attempts.

Call the method without the first two arguments if the device's LoRa module is provisioned or comes with NVS stored values. See `provision`, `saveKeys` and `restoreKeys`

## Method: `personalize`

Activate the device via ABP.

```c
bool personalize(const char *devAddr, const char *nwkSKey, const char *appSKey);
bool personalize();
```

- `const char *devAddr`: Device Address assigned to the device.
- `const char *nwkSKey`: Network Session Key assigned to the device for identification.
- `const char *appSKey`: Application Session Key assigned to the device for encryption.

Returns `true` or `false` depending on whether the activation was successful.

Call the method with no arguments if the device's LoRa module is provisioned or comes with NVS stored values. See `provisionABP`, `saveKeys` and `restoreKeys`

See the [ttn_abp](https://github.com/rgot-org/TheThingsNetwork_esp32/tree/master/examples/ttn_abp) example.

## Method: `sendBytes`

Send a message to the application using raw bytes.

```c
bool sendBytes(uint8_t *payload, size_t length, uint8_t port = 1, uint8_t confirm = 0);
```

- `uint8_t *payload `: Bytes to send.
- `size_t length`: The number of bytes. Use `sizeof(payload)` to get it.
- `uint8_t port = 1`: The port to address. Defaults to `1`.
- `uint8_t confirm = false`: Whether to ask for confirmation. Defaults to `false`

## Method: `poll`

Calls `sendBytes()` with `{ 0x00 }` as payload to poll for incoming messages.

```c
int8_t poll(uint8_t port = 1, uint8_t confirm = 0);
```

- `uint8_t  port = 1`: The port to address. Defaults to `1`.
- `uint8_t confirm = 0`: Whether to ask for confirmation.

Returns the result of `sendBytes()`.


## Method: `provision`

Sets the informations needed to activate the device via OTAA, without actually activating. Call join() without the first 2 arguments to activate.

```c
bool provision(const char *appEui, const char *appKey);
bool provision(const char *devEui, const char *appEui, const char *appKey);
```

- `const char *appEui`: Application Identifier for the device.
- `const char *appKey`: Application Key assigned to the device.
- `const char *devEui`: Device EUI.
## Method: `provisionABP`

Sets the informations needed to activate the device via ABP, without actually activating. call `personalize()` without arguments to activate.
```c
bool provisionABP(const char *devAddr, const char *nwkSKey, const char *appSKey);
```
- `const char *devAddr`: Device Address.
- `const char *nwkSKey`: Network Session Key.
- `const char *appSKey`: Application Session Key.
## Method: `saveKeys`
Save the provisioning keys (OTAA and ABP) in Non Volatile Storage (NVS). 
```c
bool saveKeys();
```
## Method: `restoreKeys`
Restore the keys from NVS and provisioning the informations for OTAA or ABP connection. Call `join()` or `Personalize()` after this method to activate the device.
```c
boobool restoreKeys(bool silent=true);
```
- `bool silent=true`: silent mode (no log)



