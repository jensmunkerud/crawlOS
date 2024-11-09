#include <Arduino.h>
#include <ArduinoBLE.h>
#include <Wire.h>
#include <secrets.h>
#include <Adafruit_I2CDevice.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_SSD1306.h>
// #include <Servo.h>
// Adafruit_SSD1306 display(128, 64, &Wire, -1);
int slaveID1 = 1;
int slaveID2 = 2;
#define I2C_ADDRESS 0x60
Adafruit_I2CDevice i2c_dev = Adafruit_I2CDevice(I2C_ADDRESS);

// PARAMETERS
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
unsigned long lastBLEPackage = 0;
int16_t xData = 0;
int16_t yData = 0;
int8_t exInner = 0;			// 1 extend, 0 nothing, -1 retract
int8_t exOuter = 0;			// 1 extend, 0 nothing, -1 retract
int8_t smooth = 1;			// 0 instant, 1 default smoothness
int8_t switcher = 0;		// 0 both, 1 lower, 2 upper
int RPMCounts[4];
bool dataReceived;
int talkingSlaves[2] = {slaveID1, slaveID2};

// Sets bluetooth characteristics
const char * serviceUUID = sUUID;
const char * xDataUUID = xUUID;
const char * yDataUUID = yUUID;
const char * exInnerUUID = extendInnerUUID;
const char * exOuterUUID = extendOuterUUID;
const char * smoothUUID = smothUUID;
const char * stageUUID = stagUUID;
BLEService joystickService(serviceUUID);
BLEIntCharacteristic xCharacteristic(xDataUUID, BLEWriteWithoutResponse);
BLEIntCharacteristic yCharacteristic(yDataUUID, BLEWriteWithoutResponse);
BLEIntCharacteristic exInnerCharacteristic(exInnerUUID, BLEWriteWithoutResponse);
BLEIntCharacteristic exOuterCharacteristic(exOuterUUID, BLEWriteWithoutResponse);
BLEIntCharacteristic smoothCharacteristic(smoothUUID, BLEWrite);
BLEIntCharacteristic stageCharacteristic(stageUUID, BLEWrite);

struct I2CData {
	int x;
	int y;
	int smooth;
	int exOuter;
	int exInner;
};

// PROGRAM START
void advertiseBLE() {
	// SETS BLUETOOTH PARAMETERS
	BLE.setLocalName("CRAWLER");
	BLE.setAdvertisedService(joystickService);
	joystickService.addCharacteristic(xCharacteristic);
	joystickService.addCharacteristic(yCharacteristic);
	joystickService.addCharacteristic(exInnerCharacteristic);
	joystickService.addCharacteristic(exOuterCharacteristic);
	joystickService.addCharacteristic(smoothCharacteristic);
	joystickService.addCharacteristic(stageCharacteristic);
	BLE.addService(joystickService);
	BLE.advertise();
	Serial.println("BLE is advertising...");
}

// void startScreen() {
// 	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
// 	display.clearDisplay();
// 	display.display();
// 	display.setTextColor(1);
// 	display.setTextSize(2);
// }

// void updateScreen() {
// 	display.clearDisplay();
// 	display.setCursor(0, 0);
// 	// display.print("x = ");
// 	// display.print(xData);
// 	// display.setCursor(0, 20);
// 	// display.print("y = ");
// 	// display.print(yData);
// 	display.print("mot1: ");
// 	display.print(RPMCounts[0]);
// 	display.setCursor(0, 20);
// 	display.print("mot2: ");
// 	display.print(RPMCounts[1]);
// 	display.display();
// }

// Transmit XY data to I2C device
void sendSlaveInstance(I2CData I2CData, int slave) {
	Wire.beginTransmission(slave);
	String data = 
	String(I2CData.x)+","+
	String(I2CData.y)+","+
	String(I2CData.smooth)+","+
	String(I2CData.exInner)+","+
	String(I2CData.exOuter);
	Wire.write(data.c_str());
	// unsigned long start = millis();
	// while (Wire.endTransmission() != 0) {  // Check if transmission is successful
	// 	if (millis() - start > 10000) {  // Timeout after 100 ms
	// 		Serial.print("I2C timeout on slave "); Serial.println(slave);
	// 		break;
	// 	}
	// }
	Wire.endTransmission();
	Serial.print("Wrote to slave "); Serial.println(slave);
	// Serial.print("Sent x: "); Serial.print(int(I2CData.x)); Serial.print("    y: "); Serial.println(int(I2CData.y));
}

// Switches between upper, lower or both stages
void sendSlave(I2CData I2CData, int switchLoc) {
	// switcher 0 both, 1 upper, -1 lower
	if (switchLoc == 1) {
		sendSlaveInstance(I2CData, slaveID1);
	}
	if (switchLoc == 2) {
		sendSlaveInstance(I2CData, slaveID2);
	} else {
		sendSlaveInstance(I2CData, slaveID1);
		sendSlaveInstance(I2CData, slaveID2);
	}
}


void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	Wire.begin();					// Starts I2C as MASTER
	Wire.setTimeout(3000);
	if (!i2c_dev.begin()) {
		Serial.print("Did not find device at 0x");
		Serial.println(i2c_dev.address(), HEX);
		while (1);
	}
	Serial.begin(9600);
	// startScreen();					// Starts screen
	// updateScreen();					// Initilializes screen
	// sendSlave({0, 0, 1, 0, 0}, 0);		// Stops all motors // THIS BUGS OUT ON STARTUP
	// Starts Bluetooth
	if (!BLE.begin()) {
		Serial.println("starting BLE failed!");
		while (1);
	}
	advertiseBLE();
}


// Read the XY data sent by the joystick, and forwarding to slaves
void receiveBLEData() {
	dataReceived = false;
	if (xCharacteristic.written()) {
		xCharacteristic.readValue((int16_t*)&xData, sizeof(int16_t));
		dataReceived = true;
	}
	if (yCharacteristic.written()) {
		yCharacteristic.readValue((int16_t*)&yData, sizeof(int16_t));
		dataReceived = true;
	}
	if (exOuterCharacteristic.written()) {
		exOuterCharacteristic.readValue((int8_t*)&exOuter, sizeof(int8_t));
		// Serial.print("Received extend OUTER: "); Serial.println(exOuter);
		dataReceived = true;
	}
	if (exInnerCharacteristic.written()) {
		exInnerCharacteristic.readValue((int8_t*)&exInner, sizeof(int8_t));
		// Serial.print("Received extend INNER: "); Serial.println(exInner);
		dataReceived = true;
	}
	if (smoothCharacteristic.written()) {
		// Smoothness control, 1 = smooth, 0 = instant
		smoothCharacteristic.readValue((int8_t*)&smooth, sizeof(int8_t));
		// Serial.print("Received SMOOTH: "); Serial.println(exOuter);
		dataReceived = true;
	}
	if (stageCharacteristic.written()) {
		stageCharacteristic.readValue((int8_t*)&switcher, sizeof(int8_t));
		// Serial.print("Received STAGE: "); Serial.println(switcher);
		dataReceived = true;
	}
	if (dataReceived) {
		I2CData dat = {xData, yData, smooth, exOuter, exInner};
		sendSlave(dat, switcher);
		lastBLEPackage = millis();
	}
}

void loop() {
	// Waits for BLE device to connect
	BLEDevice central = BLE.central();
	if (central) {
		Serial.println("Connected to central device");
		digitalWrite(LED_BUILTIN, HIGH);
		lastBLEPackage = millis();
		while (central.connected()) {
			// While the phone is connected
			receiveBLEData();

			// Happens once a second while connected
			unsigned long currentMillis = millis();
			// if (currentMillis - previousMillis >= 100) {
			// 	previousMillis = currentMillis;
			// 	Wire.requestFrom(slaveID1, 4);
			// 	int i = 0;
			// 	while (Wire.available() && i < 4) {
			// 		RPMCounts[i] = (int8_t)Wire.read();
			// 		i++;
			// 	}
			// 	updateScreen();
			// }
			if (currentMillis - lastBLEPackage > 10000) {
				Serial.println("Bluetooth timed out");
				BLE.disconnect();
				if (!central.connected()) break;  // Confirm disconnection
			}
		}
	Serial.println("Disconnected from central device");
	digitalWrite(LED_BUILTIN, LOW);
	advertiseBLE();
	sendSlave({0, 0, 1, 0, 0}, 0);		// Stops all motors
	xData = 0; yData = 0;
	// updateScreen();
	}
	// Blinks LED_BUILTIN when we are searching for bluetooth devices
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= 1000) {
		previousMillis = currentMillis;
		digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
	}
}
	
