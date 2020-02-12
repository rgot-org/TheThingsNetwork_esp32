// Lorawan_esp32.h

#ifndef _LORAWAN_ESP32_h
#define _LORAWAN_ESP32_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif
#include "lmic/arduino_lmic_hal_boards.h"
#include "lmic/lmic/oslmic.h"

class Lorawan_esp32
{
 public:
	 Lorawan_esp32();
	 void begin();
	 bool begin(const TTN_esp32_LMIC::HalPinmap_t * pPinmap);
	 void begin(uint8_t nss, uint8_t rxtx, uint8_t rst, uint8_t dio0, uint8_t dio1, uint8_t dio2);
	 bool sendBytes(uint8_t *payload, size_t length, uint8_t port = 1, uint8_t confirm = 0);
	 void sendBytesAtInterval(uint8_t *payload, size_t length, unsigned interval = 60, uint8_t port = 1, uint8_t confirm = 0);
	 static void txMessage(osjob_t* j);
	 bool txBytes(uint8_t *payload, size_t length, uint8_t port, uint8_t confirm);
	 int8_t poll(uint8_t port = 1, uint8_t confirm = 0);
	 bool haveKeys();
	 static String getMac();
	 static bool join();
	 void personalize(u4_t netID, u4_t DevAddr, uint8_t* NwkSKey, uint8_t* AppSKey);
	 bool setDataRate(uint8_t DR = 7);
	 static bool stopTNN(void);
	 void static onMessage(void(*cb)(const uint8_t *payload, size_t size, int rssi));
	 void setInterval(const unsigned TX_INTERVAL);
	 static Lorawan_esp32 *GetInstance()
	 {
		 return _pLoRaWAN;
	 }
	 static uint8_t getPort() {
		 return _port;
	 }
	 static TaskHandle_t* TTN_task_Handle;
	 static bool _joined;
	 bool joined();
	 //static void(*messageCallback)(const uint8_t *payload, size_t size, uint8_t port);
private:
	 static Lorawan_esp32 *_pLoRaWAN;
	 static uint8_t* _message;
	 static uint8_t _length, _port, _confirm;
	 static unsigned _interval;
	 static void loopStack(void * parameter);
	bool have_keys = false;
	
};



#endif

