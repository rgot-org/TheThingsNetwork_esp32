#include <TTN_CayenneLPP.h>
#include <heltec.h>
#include <TTN_esp32.h>
#include <Wire.h>  


//const char *devEui = "CHANGE_ME"; //changer avec de devEui TTN
//const char *appEui = "CHANGE_ME"; // changer avec le appEui TTN
//const char *appKey = "CHANGE_ME"; // changer avec le appKey TTN
const char *devEui = "00F62EA351D65653";
const char *appEui = "70B3D57ED0015575";
const char *appKey = "FA9E690FB5B82757797E39BF3951703D";

TTN_esp32 ttn;
TTN_CayenneLPP lpp;
SSD1306Wire  aff(0x3c, SDA_OLED, SCL_OLED, RST_OLED,GEOMETRY_64_32);
void message(const uint8_t *payload, size_t size, uint8_t port)
{
	Serial.println("-- MESSAGE");
	Serial.print("Received " + String(size) + " bytes on port " + String(port) + ":");

	for (int i = 0; i < size; i++)
	{
		Serial.print(" " + String(payload[i]));
	}
	Serial.println();
}

void setup() {
	pinMode(Vext, OUTPUT);
	pinMode(LED, OUTPUT);

	digitalWrite(Vext, LOW);
	aff.init();
	aff.flipScreenVertically();
	aff.setFont(ArialMT_Plain_10);
	
	Serial.begin(115200);
	Serial.println("Starting");
	ttn.begin();
	ttn.onMessage(message);
	unsigned long time = millis();
	ttn.join(devEui, appEui, appKey);
	Serial.printf("sda: %d, scl: %d, rst: %d\n", SDA_OLED, SCL_OLED, RST_OLED);
	aff.drawString(0, 0, "Joining TTN...");
	aff.display();
	Serial.print("joining TTN ");
	while (!ttn.joined()) {
		Serial.print(".");
		delay(500);
	}

	Serial.printf("\njoined in %ds\n", (millis()-time)/1000);
	ttn.showStatus();
}

void loop() {
	static float nb = 18.2;
	nb += 0.1;
	lpp.reset();
	lpp.addTemperature(1, nb);
	if (ttn.sendBytes(lpp.getBuffer(), lpp.getSize())) {
		Serial.printf("Temp: %f TTN_CayenneLPP: %d %x %02X%02X\n", nb, lpp.getBuffer()[0], lpp.getBuffer()[1], lpp.getBuffer()[2], lpp.getBuffer()[3]);
		aff.clear();
		aff.drawString(0, 0, "Temp:"+(String)nb+"°C");
		aff.display();
	}
	delay(30000);
}
