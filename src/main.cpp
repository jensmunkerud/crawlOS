#include <Arduino.h>
#include <Servo.h>
#include <ArduinoBLE.h>

// Sets pins
#define mot1	2
#define mot1Dir	3
#define mot2	4
#define mot2Dir	5
#define mot3	6
#define mot3Dir	7
#define mot4	8
#define mot4Dir	9
#define ser1	21
#define ser2	22
#define ser3	23

// PARAMETERS
float engineSpeed = 1;
float turningSpeed = 1;
int mappedX;
int mappedY;

Servo Servo1;
Servo Servo2;
Servo Servo3;

const char * serviceUUID = "ceeeeeee-c666-499f-b917-352312f159c5";
const char * xDataUUID = "aaaaaaaa-d2a0-44c8-a271-69ef24094b01";
const char * yDataUUID = "bbbbbbbb-f0a9-4623-b503-ee7804fca301";
BLEService joystickService(serviceUUID);
BLEIntCharacteristic xCharacteristic(xDataUUID, BLEWrite); // String characteristic for X, Y data
BLEIntCharacteristic yCharacteristic(yDataUUID, BLEWrite); // String characteristic for X, Y data

// MUST USE PIN 2 FOR THESE TYPES OF INPUTS (I THINK)
const int FG_PIN = 13; // Interrupt pin for FG signal (digital pin 2 on Arduino UNO)
const int pulsesPerRevolution = 6; // 6 FG pulses per revolution
unsigned long previousMillis = 0;  // To store the last time calculation was done
const unsigned long interval = 1000; // Interval for calculating RPM (1 second)
volatile int pulseCount = 0;  // Pulse counter
float rpm = 0;				// Motor speed in RPM

// Interrupt Service Routine (ISR) to count pulses
// void countPulses() {
// 	pulseCount++;
// }
// int getRPM(int pin) {
// 	unsigned long currentMillis = millis();
// 	int count = 0;
// 	if (currentMillis - previousMillis >= interval) {
// 	// Disable interrupts to safely read pulseCount
// 		noInterrupts();
// 		count = pulseCount;
// 		pulseCount = 0;  // Reset the counter
// 		interrupts();
// 	}
// 	previousMillis = currentMillis;
// 	// Calculate RPM
// 	return rpm = (count / (float)pulsesPerRevolution) * (60.0 / (interval / 1000.0));
// }

// PROGRAM START
void setup() {
	pinMode(mot1, OUTPUT);
	pinMode(mot2, OUTPUT);
	pinMode(mot3, OUTPUT);
	pinMode(mot4, OUTPUT);
	pinMode(FG_PIN, INPUT);
	pinMode(LED_BUILTIN, OUTPUT);
	Servo1.attach(ser1);
	Servo2.attach(ser2);
	Servo3.attach(ser3);

	Serial.begin(9600);
	// attachInterrupt(digitalPinToInterrupt(FG_PIN), countPulses, RISING);

	// STARTS BLUETOOTH
	if (!BLE.begin()) {
		Serial.println("starting BLE failed!");
		while (1);
	}
	// SETS BLUETOOTH PARAMETERS
	BLE.setLocalName("RP2040 Joystick");
	BLE.setAdvertisedService(joystickService);
	joystickService.addCharacteristic(xCharacteristic);
	joystickService.addCharacteristic(yCharacteristic);
	BLE.addService(joystickService);
	BLE.advertise();
}

class Motor {
	public:
		int mySpeed;
		int myPin;
		Motor(int pin) {
			myPin = pin;
		}

	void update(int speed) {
		analogWrite(myPin, speed);
	}

	void stop() {
		update(0);
	}
};
Motor motors[] = {Motor(mot1), Motor(mot2), Motor(mot3), Motor(mot4)};

void updateQuadDrive(int x, int y) {
	// Handle controlling actual output of 4 motors
	for (int i = 0; i < 4; i++) {
		motors[i].update(y);
	}
}

void loop() {
	// LOOKS FOR BLUETOOTH DEVICES
	BLEDevice central = BLE.central();
	if (central) {
		// While the phone is connected
		Serial.println("Connected to central device");
		digitalWrite(LED_BUILTIN, HIGH);
		while (central.connected()) {
			// Read the X, Y data sent by the joystick app
			if (xCharacteristic.written()) {
				int8_t xData = xCharacteristic.value();
				mappedX = map(xData, -126, 126, -255, 255);
				Serial.print("X: "); Serial.println(mappedX);
			}
			if (yCharacteristic.written()) {
				int8_t yData = yCharacteristic.value();
				mappedY = map(yData, -126, 126, -255, 255);
				Serial.print("Y: "); Serial.println(mappedY);
			}
			updateQuadDrive(mappedX, mappedY);
		}
	// Important to keep the {} this way, since then this code runs ONCE after disconnection, not in the "actual" loop...
	Serial.println("Disconnected from central device");
	digitalWrite(LED_BUILTIN, LOW);
	for (int i = 0; i < 4; i++) {
		motors[i].update(0);
	}
	}
}
