// 
// 
// 

#include "lorawan_esp32.h"
#include "lmic/lmic.h"
#include "lmic/arduino_lmic_hal_boards.h"
#include "lmic/hal/hal.h"
#define LOOP_TTN_MS 1
//#define DEBUG

static unsigned TX_INTERVAL = 0;
static bool cyclique = false;
uint8_t Lorawan_esp32::_confirm = 0;
uint8_t Lorawan_esp32::_port = 1;
uint8_t* Lorawan_esp32::_message = 0;
uint8_t Lorawan_esp32::_length = 1;
bool Lorawan_esp32::_joined = false;
TaskHandle_t* Lorawan_esp32::TTN_task_Handle = NULL;
void(*messageCallback)(const uint8_t *payload, size_t size, uint8_t port);


//static AT_SettingsClass at;


//#if defined(TTN_HAS_AT_COMMANDS)
//void ttn_provisioning_task_caller(void* pvParameter);
//#endif
static osjob_t sendjob;
static osjobcb_t sendMsg;
// --- Constructor

Lorawan_esp32::Lorawan_esp32()
{
	
}
void Lorawan_esp32::begin()
{
	// record self in a static so that we can dispatch events
	//ASSERT(Lorawan_esp32::pLoRaWAN == this ||Lorawan_esp32::pLoRaWAN == NULL);
	//pLoRaWAN = this;
	os_init_ex(TTN_esp32_LMIC::GetPinmap_ThisBoard());
	LMIC_reset();
}
bool Lorawan_esp32::begin(const TTN_esp32_LMIC::HalPinmap_t * pPinmap)
{
	if (!os_init_ex(pPinmap))
		return false;

	// Reset the MAC state. Session and pending data transfers will be
	// discarded.
	LMIC_reset();
	return true;
}
bool Lorawan_esp32::joined() {
	return _joined;
}
void Lorawan_esp32::begin(uint8_t nss, uint8_t rxtx, uint8_t rst, uint8_t dio0, uint8_t dio1, uint8_t dio2) {
	static	const TTN_esp32_LMIC::HalPinmap_t myPinmap =
	{
	.nss = nss,
	.rxtx = rxtx,
	.rst = rst,
	.dio = {dio0, dio1, dio2},

	};
	os_init_ex(&myPinmap);
	LMIC_reset();
}
void Lorawan_esp32::setInterval(unsigned INTERVAL = 60) {
	/*_interval=INTERVAL;*/
	TX_INTERVAL = INTERVAL;
}
void Lorawan_esp32::sendBytesAtInterval(uint8_t *payload, size_t length, unsigned interval, uint8_t port, uint8_t confirm) {
	cyclique = (interval != 0) ? true : false;
	TX_INTERVAL = interval;
	txBytes(payload, length, port, confirm);

}
bool Lorawan_esp32::sendBytes(uint8_t *payload, size_t length, uint8_t port, uint8_t confirm) {
	cyclique = false;
	return txBytes(payload, length, port, confirm);
}
int8_t Lorawan_esp32::poll(uint8_t port, uint8_t confirm) {
	return sendBytes(0, 1, port, confirm);
}
bool Lorawan_esp32::txBytes(uint8_t *payload, size_t length, uint8_t port, uint8_t confirm) {

	Lorawan_esp32::_message = payload;
	_length = length;
	_port = port;
	_confirm = confirm;

	txMessage(&sendjob);
	if ((LMIC.opmode & OP_TXDATA) == 0)
	{
		return false;
	}

	return true;

}
void Lorawan_esp32::txMessage(osjob_t* j) {
	if (_joined) {
		Lorawan_esp32 * const pLoRaWAN = Lorawan_esp32::GetInstance();
		// Check if there is not a current TX/RX job running

		if (LMIC.opmode & OP_TXRXPEND) {
#ifdef DEBUG
			Serial.println(F("OP_TXRXPEND, not sending"));
#endif // DEBUG		

		}
		else {
			// Prepare upstream data transmission at the next possible time.
			LMIC_setTxData2(pLoRaWAN->_port, pLoRaWAN->_message, pLoRaWAN->_length, pLoRaWAN->_confirm);
#ifdef DEBUG
			Serial.println(F("Packet queued"));
#endif // DEBUG	
		}
		// Next TX is scheduled after TX_COMPLETE event.
	}
#ifdef DEBUG
	else Serial.println("pas de jonction au reseau");
#endif // DEBUG

}


String Lorawan_esp32::getMac() {
	uint8_t mac[6];
	esp_err_t err = esp_efuse_mac_get_default(mac);
	char buf[20];
	for (size_t i = 0; i < 5; i++)
	{
		sprintf(buf + (3 * i), "%02X:", mac[i]);
	}
	sprintf(buf + (3 * 5), "%02X:", mac[5]);

	return String(buf);
}


void onEvent(ev_t ev) {
	Lorawan_esp32 * const pLoRaWAN = Lorawan_esp32::GetInstance();
#ifdef DEBUG
	Serial.print(os_getTime());
	Serial.print(": ");
#endif // DEBUG


	switch (ev) {
	case EV_SCAN_TIMEOUT:
#ifdef DEBUG
		Serial.println(F("EV_SCAN_TIMEOUT"));
#endif // DEBUG


		break;
	case EV_BEACON_FOUND:
#ifdef DEBUG
		Serial.println(F("EV_BEACON_FOUND"));
#endif // DEBUG
		break;
	case EV_BEACON_MISSED:
#ifdef DEBUG
		Serial.println(F("EV_BEACON_MISSED"));
#endif // DEBUG
		break;
	case EV_BEACON_TRACKED:
#ifdef DEBUG
		Serial.println(F("EV_BEACON_TRACKED"));
#endif // DEBUG
		break;
	case EV_JOINING:
		pLoRaWAN->_joined = false;
#ifdef DEBUG
		Serial.println(F("EV_JOINING"));
#endif // DEBUG
		break;
	case EV_JOINED:
#ifdef DEBUG
		Serial.println(F("EV_JOINED"));
		{
			u4_t netid = 0;
			devaddr_t devaddr = 0;
			u1_t nwkKey[16];
			u1_t artKey[16];
			LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
			Serial.print("netid: ");
			Serial.println(netid, DEC);
			Serial.print("devaddr: ");
			Serial.println(devaddr, HEX);
			Serial.print("artKey: ");
			for (size_t i = 0; i < sizeof(artKey); ++i) {
				Serial.print(artKey[i], HEX);
			}
			Serial.println("");
			Serial.print("nwkKey: ");
			for (size_t i = 0; i < sizeof(nwkKey); ++i) {
				Serial.print(nwkKey[i], HEX);
			}

		}
#endif // DEBUG
		pLoRaWAN->_joined = true;
		// Disable link check validation (automatically enabled
		// during join, but because slow data rates change max TX
	// size, we don't use it in this example.

		LMIC_setLinkCheckMode(0);
		break;
		/*
		|| This event is defined but not used in the code. No
		|| point in wasting codespace on it.
		||
		|| case EV_RFU1:
		||     Serial.println(F("EV_RFU1"));
		||     break;
		*/
	case EV_JOIN_FAILED:
#ifdef DEBUG
		Serial.println(F("EV_JOIN_FAILED"));
#endif // DEBUG
		pLoRaWAN->_joined = false;
		break;
	case EV_REJOIN_FAILED:
#ifdef DEBUG
		Serial.println(F("EV_REJOIN_FAILED"));
#endif // DEBUG
		break;
	case EV_TXCOMPLETE:
#ifdef DEBUG
		Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
		if (LMIC.txrxFlags & TXRX_ACK)
			Serial.println(F("Received ack"));
#endif // DEBUG
		if (LMIC.dataLen) {
#ifdef DEBUG
			//messageCallback(downlink, downlinkLength, downlinkPort);
			Serial.print(F("Received "));
			Serial.print(LMIC.dataLen);
			Serial.println(F(" bytes of payload"));
			String JSONMessage = "";
			for (byte i = LMIC.dataBeg; i < LMIC.dataBeg + LMIC.dataLen; i++)
			{
				JSONMessage += (char)LMIC.frame[i];
			}
			Serial.println(JSONMessage);
#endif // DEBUG

			uint8_t downlink[LMIC.dataLen];
			for (byte i = LMIC.dataBeg; i < LMIC.dataBeg + LMIC.dataLen; i++) {
				downlink[i - LMIC.dataBeg] = LMIC.frame[i];
			}
			if (messageCallback)
			{
				messageCallback(downlink, LMIC.dataLen, LMIC.dataBeg - 1);
			}

		}

		// Schedule next transmission
	/*	if (cyclique)
		{
			os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), pLoRaWAN->txMessage);
		}*/

		break;
	case EV_LOST_TSYNC:
#ifdef DEBUG
		Serial.println(F("EV_LOST_TSYNC"));
#endif // DEBUG
		break;
	case EV_RESET:
#ifdef DEBUG
		Serial.println(F("EV_RESET"));
#endif // DEBUG
		pLoRaWAN->_joined = false;
		break;
	case EV_RXCOMPLETE:
#ifdef DEBUG
		// data received in ping slot
		Serial.println(F("EV_RXCOMPLETE"));
#endif // DEBUG
		break;
	case EV_LINK_DEAD:
		pLoRaWAN->_joined = false;
#ifdef DEBUG
		Serial.println(F("EV_LINK_DEAD"));
#endif // DEBUG
		break;
	case EV_LINK_ALIVE:
#ifdef DEBUG
		Serial.println(F("EV_LINK_ALIVE"));
#endif // DEBUG
		break;
		/*
		|| This event is defined but not used in the code. No
		|| point in wasting codespace on it.
		||
		|| case EV_SCAN_FOUND:
		||    Serial.println(F("EV_SCAN_FOUND"));
		||    break;
		*/
	case EV_TXSTART:
#ifdef DEBUG
		Serial.println(F("EV_TXSTART"));
#endif // DEBUG
		break;


	default:
#ifdef DEBUG
		Serial.print(F("Unknown event: "));
		Serial.println((unsigned)ev);
#endif // DEBUG
		break;
	}

}
void Lorawan_esp32::onMessage(void(*cb)(const uint8_t *payload, size_t size, uint8_t port))
{
	messageCallback = cb;
}




// --- Key handling

bool Lorawan_esp32::haveKeys()
{
	return have_keys;
}

void Lorawan_esp32::personalize(u4_t netID, u4_t DevAddr, uint8_t* NwkSKey, uint8_t* AppSKey) {

	LMIC_setClockError(MAX_CLOCK_ERROR * 7 / 100);
	LMIC_reset();
	LMIC_setSession(netID,DevAddr,NwkSKey,AppSKey);
#if defined(CFG_eu868)
	// Set up the channels used by the Things Network, which corresponds
	// to the defaults of most gateways. Without this, only three base
	// channels from the LoRaWAN specification are used, which certainly
	// works, so it is good for debugging, but can overload those
	// frequencies, so be sure to configure the full frequency range of
	// your network here (unless your network autoconfigures them).
	// Setting up channels should happen after LMIC_setSession, as that
	// configures the minimal channel set.
	// NA-US channels 0-71 are configured automatically
	LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
	LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
	LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
	LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
	LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
	LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
	LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
	LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);      // g-band
	LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK), BAND_MILLI);      // g2-band
#endif
		// Disable link check validation
	LMIC_setLinkCheckMode(0);

	// TTN uses SF9 for its RX2 window.
	LMIC.dn2Dr = DR_SF9;

	// Set data rate and transmit power for uplink
	LMIC_setDrTxpow(DR_SF7, 14);
	// Start job
	_joined = true;
	xTaskCreatePinnedToCore(loopStack, "ttnTask", 2048, (void *)1, (5 | portPRIVILEGE_BIT), TTN_task_Handle, 1);
}

bool Lorawan_esp32::join() {
	LMIC_setClockError(MAX_CLOCK_ERROR * 7 / 100);
	LMIC_unjoin();
	LMIC_startJoining();
	
#ifdef DEBUG
	Serial.println("_joined");
#endif // DEBUG	

	xTaskCreatePinnedToCore(loopStack, "ttnTask", 2048, (void *)1, (5 | portPRIVILEGE_BIT), TTN_task_Handle, 1);
	return true;
}

bool Lorawan_esp32::setDataRate(uint8_t DR) {
	LMIC.datarate = DR;
}
void Lorawan_esp32::loopStack(void * parameter) {
	for (;;) {
		os_runloop_once();
		vTaskDelay(LOOP_TTN_MS / portTICK_PERIOD_MS);
	}

}

bool Lorawan_esp32::stopTNN() {
	if (TTN_task_Handle != NULL) {
		vTaskDelete(TTN_task_Handle);
	}
}

