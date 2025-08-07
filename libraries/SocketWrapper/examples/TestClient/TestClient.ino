#include <ZephyrClient.h>
#include "Ethernet.h"

ZephyrClient client;
IPAddress addr(93, 184, 215, 14);

void setup() {

	pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(115200);

	while (Ethernet.linkStatus() != LinkON) {
		Serial.println("waiting for link on");
		delay(100);
	}
	Ethernet.begin();
	Serial.println(Ethernet.localIP());

	auto res = client.connect("example.com", 80);
	res = client.println("GET / HTTP/1.0");
	client.println("Host: example.com");
	client.println("Connection: close");
	client.println();
}

// the loop function runs over and over again forever
void loop() {
	while (client.available()) {
		Serial.write(client.read());
	}
}
