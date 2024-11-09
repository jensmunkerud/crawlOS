#include <Arduino.h>
#include <ArduinoBLE.h>
#include <SPI.h>
#include <secrets.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// #include <Servo.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);
int slavePin1 = 10;
int slavePin2 = 9;

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
int rpmData[2][4] = {{0, 0, 0, 0},
					 {0, 0, 0, 0}};
bool dataReceived;

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

struct SPIData {
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

void startScreen() {
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	display.clearDisplay();
	display.display();
	display.setTextColor(1);
	display.setTextSize(2);
	Wire.begin();
}

void updateScreen() {
	display.clearDisplay();
	display.setCursor(0, 0);
	// display.print("x = ");
	// display.print(xData);
	// display.setCursor(0, 20);
	// display.print("y = ");
	// display.print(yData);
	display.print("mot1: ");
	display.print(rpmData[0][0]);
	display.setCursor(0, 20);
	display.print("mot2: ");
	display.print(rpmData[0][1]);
	display.display();
}



// Transmit XY data to I2C device
void sendSlaveInstance(SPIData SPIData, int slave) {
	digitalWrite(slave, LOW); // Enables comms with selected slave
	String data = 
	String(SPIData.x)+","+
	String(SPIData.y)+","+
	String(SPIData.smooth)+","+
	String(SPIData.exInner)+","+
	String(SPIData.exOuter)+"\n";

	int dataLength = data.length();
	byte dataBytes[dataLength + 1];
	data.getBytes(dataBytes, dataLength + 1);

	SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

	for (int i = 0; i < dataLength; i++) {
		SPI.transfer(dataBytes[i]);
	}
	SPI.endTransaction();
	digitalWrite(slave, HIGH);
}

// Switches between upper, lower or both stages
void sendSlave(SPIData I2CData, int switchLoc) {
	// switcher 0 both, 1 upper, -1 lower
	if (switchLoc == 1) {
		sendSlaveInstance(I2CData, slavePin1);
	}
	if (switchLoc == 2) {
		sendSlaveInstance(I2CData, slavePin2);
	} else {
		sendSlaveInstance(I2CData, slavePin1);
		sendSlaveInstance(I2CData, slavePin2);
	}
}


void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(slavePin1, OUTPUT);		digitalWrite(slavePin1, HIGH);
	pinMode(slavePin2, OUTPUT);		digitalWrite(slavePin2, HIGH);
	SPI.begin();
	Serial.begin(9600);
	startScreen();					// Starts screen
	updateScreen();					// Initilializes screen
	sendSlave({0, 0, 1, 0, 0}, switcher);		// Stops all motors // THIS BUGS OUT ON STARTUP
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
		SPIData data = {xData, yData, smooth, exOuter, exInner};
		sendSlave(data, switcher);
		lastBLEPackage = millis();
	}
}


void getRPM(int slave) {
	SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
	digitalWrite(slave, LOW);	// Enable the slave
	SPI.transfer('r');			// Enables RPM Readback
	for (int i = 0; i < 4; i++) {
		delayMicroseconds(100);
		rpmData[slave == 9 ? 1 : 0][i] = SPI.transfer(0);
	}
	digitalWrite(slave, HIGH); // Disable the slave
	SPI.endTransaction();

	// Print received RPM values
	for (int i = 0; i < 4; i++) {
		Serial.print("  RPM ");
		Serial.print(i + 1);
		Serial.print(": ");
		Serial.print(rpmData[0][i]);
	}
	Serial.println();
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
			if (currentMillis - previousMillis >= 1000) {
				previousMillis = currentMillis;
				getRPM(slavePin1);
				updateScreen();
			}
			
			// Bluetooth timeout after 10s
			if (currentMillis - lastBLEPackage > 10000) {
				Serial.println("Bluetooth timed out");
				BLE.disconnect();
				if (!central.connected()) break;
			}
		}
	Serial.println("Disconnected from central device");
	digitalWrite(LED_BUILTIN, LOW);
	advertiseBLE();
	sendSlave({0, 0, 1, 0, 0}, 0);		// Stops all motors
	xData = 0; yData = 0;
	updateScreen();
	}
	// Blinks LED_BUILTIN when we are searching for bluetooth devices
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= 1000) {
		previousMillis = currentMillis;
		digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
	}
}
	