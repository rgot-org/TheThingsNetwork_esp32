#include <TheThingsNetwork_esp32.h>
#include <TTN_CayenneLPP.h>

const char *devEui = "CHANGE_ME";
const char *appEui = "CHANGE_ME";
const char *appKey = "CHANGE_ME";

TheThingsNetwork_esp32 ttn;
TTN_CayenneLPP lpp;

void setup() {
    Serial.begin(115200);
    Serial.println("Starting");
ttn.join(devEui, appEui, appKey);	
Serial.print("joining TTN ");
while (!ttn.joined()) {
	Serial.print(".");
	delay(500);
}
Serial.println("\njoined !");
ttn.showStatus();
}

void loop() {
	static float nb =18.2;
	nb += 0.1;
	lpp.reset();
	lpp.addTemperature(1, nb);
	if (ttn.sendBytes(lpp.getBuffer(), lpp.getSize())) {
		Serial.printf("Temp: %f TTN_CayenneLPP: %d %x %02X%02X\n",nb, lpp.getBuffer()[0], lpp.getBuffer()[1], lpp.getBuffer()[2], lpp.getBuffer()[3]);
	}	
	delay(30000);
 }
