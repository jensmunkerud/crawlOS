#include <Arduino.h>
#include <ArduinoBLE.h>
#include <Wire.h>
#include <secrets.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// #include <Servo.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);
#define I2CSlave 9

// PARAMETERS
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
int16_t xData = 0;
int16_t yData = 0;
String rawData;

// Sets bluetooth characteristics
const char * serviceUUID = sUUID;
const char * xDataUUID = xUUID;
const char * yDataUUID = yUUID;
BLEService joystickService(serviceUUID);
BLEIntCharacteristic xCharacteristic(xDataUUID, BLEWriteWithoutResponse); // String characteristic for X, Y data
BLEIntCharacteristic yCharacteristic(yDataUUID, BLEWriteWithoutResponse); // String characteristic for X, Y data

struct XY {
	int x;
	int y;
};

// PROGRAM START
void advertiseBLE() {
	// SETS BLUETOOTH PARAMETERS
	BLE.setLocalName("CRAWLER");
	BLE.setAdvertisedService(joystickService);
	joystickService.addCharacteristic(xCharacteristic);
	joystickService.addCharacteristic(yCharacteristic);
	BLE.addService(joystickService);
	BLE.advertise();
	Serial.println("BLE is advertising...");
}

void startScreen() {
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	display.clearDisplay();
	display.display();
	display.setTextColor(1);
	display.setTextSize(2);
}

void updateScreen() {
	display.clearDisplay();
	display.setCursor(0, 0);
	display.print("x = ");
	display.print(xData);
	display.setCursor(0, 20);
	display.print("y = ");
	display.print(yData);
	display.setCursor(0, 40);
	display.print(rawData);
	display.display();
}

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);

	// Starts I2C as MASTER
	Wire.begin(); 
	Serial.begin(9600);
	startScreen();
	// Starts Bluetooth
	if (!BLE.begin()) {
		Serial.println("starting BLE failed!");
		while (1);
	}
	advertiseBLE();
}

// Transmit XY data to I2C device
void sendSlave(XY XYdata) {
	Wire.beginTransmission(I2CSlave);
	String data = String(XYdata.x) + "," + String(XYdata.y);
	Wire.write(data.c_str());
	Wire.endTransmission();
	Serial.print("Sent x: "); Serial.print(int(XYdata.x)); Serial.print("    y: "); Serial.println(int(XYdata.y));
}

// Read the XY data sent by the joystick, and forwarding to slaves
void receiveBLEData() {
	if (xCharacteristic.written()) {
		xCharacteristic.readValue((int16_t*)&xData, sizeof(int16_t));
	}
	if (yCharacteristic.written()) {
		yCharacteristic.readValue((int16_t*)&yData, sizeof(int16_t));
		sendSlave({xData, yData});
	}
}

void loop() {
	// Waits for BLE device to connect
	BLEDevice central = BLE.central();
	if (central) {
		Serial.println("Connected to central device");
		digitalWrite(LED_BUILTIN, HIGH);
		while (central.connected()) {
			// While the phone is connected
			receiveBLEData();

			// Happens once a second while connected
			unsigned long currentMillis = millis();
			if (currentMillis - previousMillis >= 100) {
				previousMillis = currentMillis;
				Wire.requestFrom(I2CSlave, 20);
				while (Wire.available()) {
					char c = Wire.read();               // Read each byte from the slave
					rawData += c;
				}
				updateScreen();
				rawData = "";
			}
		}
	Serial.println("Disconnected from central device");
	digitalWrite(LED_BUILTIN, LOW);
	advertiseBLE();
	sendSlave({0, 0});
	}
	// Blinks LED_BUILTIN when we are searching for bluetooth devices
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= 1000) {
		previousMillis = currentMillis;
		digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
	}
}
	
