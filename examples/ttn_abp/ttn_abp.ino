#include <TTN_esp32.h>
#include <TTN_CayenneLPP.h>

const char *devAddr = "CHANGE_ME";
const char *NwkSKey = "CHANGE_ME";
const char *AppSKey = "CHANGE_ME";

TTN_esp32 ttn;
TTN_CayenneLPP lpp;

void message(const uint8_t *payload, size_t size, uint8_t port)
{
	Serial.println("-- MESSAGE");
	Serial.print("Received " + String(size) + " bytes on port " + String(port) + ":");

	for (int i = 0; i < size; i++)
	{
		//Serial.print(" " + String(payload[i]));
		Serial.write(payload[i]);
	}

	Serial.println();
}

void setup() {
	Serial.begin(115200);
	Serial.println("Starting");
	ttn.begin();
	ttn.onMessage(message);// declare callback function when is downlink from server
	ttn.personalize(devAddr, NwkSKey, AppSKey);
	ttn.showStatus();
}

void loop() {
	static float nb = 18.2;
	nb += 0.1;
	lpp.reset();
	lpp.addTemperature(1, nb);
	if (ttn.sendBytes(lpp.getBuffer(), lpp.getSize())) {
		Serial.printf("Temp: %f TTN_CayenneLPP: %d %x %02X%02X\n", nb, lpp.getBuffer()[0], lpp.getBuffer()[1], lpp.getBuffer()[2], lpp.getBuffer()[3]);
	}
	delay(10000);
}
