/************************************
You can use this library with a BLE app.
note the service uuid and characteristics below.
you can install the android app TTN ESP32 Provisioning
https://play.google.com/store/apps/details?id=org.rgot.BLE_TEST 
to use this library and provisioning the esp32 Lora 
****************************************/
#ifndef _TTN_BLE_ESP32_h
#define _TTN_BLE_ESP32_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <TTN_esp32.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "16f9e4cb-d966-4e2c-9b23-35cba66e4f09"
#define CHARACTERISTIC_APPKEY "c357e06e-523e-11ea-8d77-2e728ce88125"
#define CHARACTERISTIC_DEVEUI "c357e2d0-523e-11ea-8d77-2e728ce88125"
#define CHARACTERISTIC_APPEUI "c357e6cc-523e-11ea-8d77-2e728ce88125"
#define CHARACTERISTIC_DEV_ADDR "c357e848-523e-11ea-8d77-2e728ce88125"
#define CHARACTERISTIC_NWKSKEY "c357e992-523e-11ea-8d77-2e728ce88125"
#define CHARACTERISTIC_APP_SKEY "c357ebf4-523e-11ea-8d77-2e728ce88125"

class TTN_BLE_esp32 
{
 protected:


 public:
	TTN_BLE_esp32();
	void init();
	static bool begin(std::string bt_name = "");
    static bool stop();
    static void rebootESP32();
    static bool getInitialized();
};

#endif

