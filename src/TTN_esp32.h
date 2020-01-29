// TTN_esp32.h

#ifndef _TTN_ESP32_h
#define _TTN_ESP32_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif
#include "lorawan_esp32.h"
#include "lmic/lmic/oslmic.h"
#include "nvs_flash.h"

class TTN_esp32 : public Lorawan_esp32
{
public:
	TTN_esp32();
	bool saveKeys();
	bool restoreKeys(bool silent=true);
	bool haveKeys();
	//bool decodeKeys(const char *dev_eui, const char *app_eui, const char *app_key);
	//bool fromMAC(const char *app_eui, const char *app_key);	
	size_t getAppEui(char *buffer, size_t size);
	String getAppEui();
	size_t getDevEui(char *buffer, size_t size, bool hardwareEui=false);
	String getDevEui(bool hardwareEui=false);
	bool provision(const char *appEui, const char *appKey);
	bool provision(const char *devEui, const char *appEui, const char *appKey);
	bool provisionABP(const char *devAddr, const char *nwkSKey, const char *appSKey);
	bool join();
	bool join(const char *app_eui, const char *app_key, int8_t retries = -1, uint32_t retryDelay = 10000);
	bool join(const char *dev_eui, const char *app_eui, const char *app_key, int8_t retries = -1, uint32_t retryDelay = 10000);
	bool personalize(const char *devAddr, const char *nwkSKey, const char *appSKey);
	bool personalize();
	void showStatus();
	//void static onMessage(void(*cb)(const uint8_t *payload, size_t size, uint8_t port));
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

	bool have_keys = false;
};



#endif

