#include "ZephyrServer.h"
#include "Ethernet.h"

ZephyrServer server(80);

void setup() {
	Serial.begin(115200);

	while (Ethernet.linkStatus() != LinkON) {
		Serial.println("waiting for link on");
		delay(100);
	}
	Ethernet.begin();
	Serial.println(Ethernet.localIP());
	server.begin();
}

void loop() {

	ZephyrClient client = server.available();
	if (client) {
		Serial.println("new client");
		// an http request ends with a blank line
		bool currentLineIsBlank = true;
		while (client.connected()) {
			if (client.available()) {
				char c = client.read();
				Serial.write(c);
				// if you've gotten to the end of the line (received a newline
				// character) and the line is blank, the http request has ended,
				// so you can send a reply
				if (c == '\n' && currentLineIsBlank) {
					// send a standard http response header
					client.println("HTTP/1.1 200 OK");
					client.println("Content-Type: text/html");
					client.println("Connection: close"); // the connection will be closed after
														 // completion of the response
					client.println("Refresh: 5"); // refresh the page automatically every 5 sec
					client.println();
					client.println("<!DOCTYPE HTML>");
					client.println("<html>");
					// output the value of each analog input pin
					for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
						int sensorReading = random(analogChannel, 100);
						client.print("analog input ");
						client.print(analogChannel);
						client.print(" is ");
						client.print(sensorReading);
						client.println("<br />");
					}
					client.println("</html>");
					break;
				}

				if (c == '\n') {
					// you're starting a new line
					currentLineIsBlank = true;

				} else if (c != '\r') {
					// you've gotten a character on the current line
					currentLineIsBlank = false;
				}
			}
		}
		// give the web browser time to receive the data
		delay(1);
		// close the connection:
		client.stop();
		Serial.println("client disconnected");
	} else {
		delay(100);
	}
}
