/*******************************************************************************
 * 
 * ttn-esp32 - The Things Network device library for ESP-IDF / SX127x
 * 
 * Copyright (c) 2018 Manuel Bleichenbacher
 * 
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 *
 * Task listening on a UART port for provisioning commands.
 *******************************************************************************/

#ifndef _ttnprovisioning_h_
#define _ttnprovisioning_h_
#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif
#include "lmic/lmic/oslmic.h"
#include "nvs_flash.h"


class TTN_esp32_Provisioning
{
public:
    TTN_esp32_Provisioning();

    bool haveKeys();
    bool decodeKeys(const char *dev_eui, const char *app_eui, const char *app_key);
    bool fromMAC(const char *app_eui, const char *app_key);
    bool saveKeys();
    bool restoreKeys(bool silent);
	 size_t getAppEui(char *buffer, size_t size);
	  String getAppEui();
	 size_t getDevEui(char *buffer, size_t size);
	  String getDevEui(bool hardwareEui);

private:
    bool decode(bool incl_dev_eui, const char *dev_eui, const char *app_eui, const char *app_key);
    bool readNvsValue(nvs_handle handle, const char* key, uint8_t* data, size_t expected_length, bool silent);
    bool writeNvsValue(nvs_handle handle, const char* key, const uint8_t* data, size_t len);



    static bool hexStrToBin(const char *hex, uint8_t *buf, int len);
    static int hexTupleToByte(const char *hex);
    static int hexDigitToVal(char ch);
    static void binToHexStr(const uint8_t* buf, int len, char* hex);
    static char valToHexDigit(int val);
    static void swapBytes(uint8_t* buf, int len);
    static bool isAllZeros(const uint8_t* buf, int len);

private:
    bool have_keys = false;

};

#endif
