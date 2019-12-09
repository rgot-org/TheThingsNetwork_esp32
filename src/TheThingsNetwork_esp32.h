
#ifndef _TTN_ESP32_h
#define _TTN_ESP32_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif
#include "lmic/arduino_lmic_hal_boards.h"
#include "lmic/lmic/oslmic.h"
#include "TTN_esp32_Provisioning.h"

class TheThingsNetwork_esp32
{

public:

	TheThingsNetwork_esp32();

	void begin();
	bool begin(const TTN_esp32_LMIC::HalPinmap_t * pPinmap);
	void begin(uint8_t nss, uint8_t rxtx, uint8_t rst, uint8_t dio0, uint8_t dio1, uint8_t dio2);
	bool sendBytes(uint8_t *payload, size_t length, uint8_t port = 1, uint8_t confirm = 0);
	void sendBytesAtInterval(uint8_t *payload, size_t length, unsigned interval = 60, uint8_t port = 1, uint8_t confirm = 0);
	static void txMessage(osjob_t* j);
	bool txBytes(uint8_t *payload, size_t length, uint8_t port, uint8_t confirm);
	int8_t poll(uint8_t port = 1, uint8_t confirm = 0);
	bool haveKeys();
	static size_t getAppEui(char *buffer, size_t size = 8);
	static size_t getDevEui(char *buffer, size_t size = 8);
	static String getAppEui();
	static String getDevEui(bool hardwareEUI = false);
	bool saveKeys();
	bool restoreKeys();
	static String getMac();
	static void showStatus();
	bool provision(const char *app_eui, const char *app_key);
	bool provision(const char *dev_eui, const char *app_eui, const char *app_key);
	static bool join();
	static bool join(const char *app_eui, const char *app_key, int8_t retries = -1, uint32_t retryDelay = 10000);
	static bool join(const char *dev_eui, const char *app_eui, const char *app_key, int8_t retries = -1, uint32_t retryDelay = 10000);
	static bool stopTNN(void);
	//bool fromMAC(const char *app_eui, const char *app_key);
	void static onMessage(void(*cb)(const uint8_t *payload, size_t size, uint8_t port));
	//bool saveKeys();
	//bool restoreKeys(bool silent);
	void setInterval(const unsigned TX_INTERVAL);
	static TheThingsNetwork_esp32 *GetInstance()
	{
		return pLoRaWAN;
	}
	static uint8_t getPort() {
		return _port;
	}
	bool joined();
	static TaskHandle_t* TTN_task_Handle;
	static bool _joined;
	
private:
	static TheThingsNetwork_esp32 *pLoRaWAN;

	static uint8_t* _message;
	static uint8_t _length, _port, _confirm;
	static unsigned _interval;
	static void loopStack(void * parameter);

private:
	bool have_keys = false;

};

#endif

