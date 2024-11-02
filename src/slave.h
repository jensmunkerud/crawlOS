#include <Wire.h>

// Sets pins
#define mot1		3
#define mot1Dir		2
#define mot2		5
#define mot2Dir		4
#define mot3		6
#define mot3Dir		7
#define mot4		9
#define mot4Dir		8

#define pistonInner	10
#define pistonOuter	11

// #define MOTOR1_FG_PIN A0
// #define MOTOR2_FG_PIN A1
// #define MOTOR3_FG_PIN A2
// #define MOTOR4_FG_PIN A3

#define I2C_ID		9

// // Variables to store pulse counts and last state for each motor
// unsigned long pulseCount1 = 0;
// unsigned long pulseCount2 = 0;
// unsigned long pulseCount3 = 0;
// unsigned long pulseCount4 = 0;

// int lastState1 = LOW;
// int lastState2 = LOW;
// int lastState3 = LOW;
// int lastState4 = LOW;

// unsigned long lastMillis = 0; // Last time RPM was calculated
// const unsigned long rpmCalcInterval = 1000; // Interval to calculate RPM (in ms)

// // RPM results
// float rpm1 = 0;
// float rpm2 = 0;
// float rpm3 = 0;
// float rpm4 = 0;

// PARAMETERS
float smoothness = 0;		// units / sec		(disable = 0)
float currentX = 0;
float currentY = 0;
bool hasUpdated;
unsigned long currentMillis;
unsigned long previousMillis = 0;

int x, y;
String rawData;
int last = 0;

// const int FG_PIN = 13; // Interrupt pin for FG signal (digital pin 2 on Arduino UNO)
// const int pulsesPerRevolution = 6; // 6 FG pulses per revolution
// unsigned long previousMillis = 0;	// To store the last time calculation was done
// const unsigned long interval = 1000; // Interval for calculating RPM (1 second)
// volatile int pulseCount = 0;	// Pulse counter
// float rpm = 0;				// Motor speed in RPM
// // Interrupt Service Routine (ISR) to count pulses
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
// 		pulseCount = 0;	// Reset the counter
// 		interrupts();
// 	}
// 	previousMillis = currentMillis;
// 	// Calculate RPM
// 	return rpm = (count / (float)pulsesPerRevolution) * (60.0 / (interval / 1000.0));
// }

void getRPM() {
	Wire.write(String("akbar").c_str());
}

void setup() {
	// pinMode(FG_PIN, INPUT);
	// attachInterrupt(digitalPinToInterrupt(FG_PIN), countPulses, RISING);
	// pinMode(MOTOR1_FG_PIN, INPUT);
	// pinMode(MOTOR2_FG_PIN, INPUT);
	// pinMode(MOTOR3_FG_PIN, INPUT);
	// pinMode(MOTOR4_FG_PIN, INPUT);
	// Starts I2C as SLAVE 9
	Wire.begin(I2C_ID);
	Wire.onReceive(getData);
	Wire.onRequest(getRPM);
	Serial.begin(9600);
}

class Motor {
	public:
		int mySpeed;
		int myPin;
		int myDirPin;
		Motor(int motPin, int dirPin) {
			myPin = motPin;
			myDirPin = dirPin;
			pinMode(myPin, OUTPUT);	pinMode(myDirPin, OUTPUT);
		}

	void update(int speed) {
		if (mySpeed > 0) {
			pinMode(myDirPin, OUTPUT);	// CW Rotation
			// analogWrite(myDirPin, LOW);		// Try without this line
		} else {
			pinMode(myDirPin, INPUT);	// CCW Rotation
		}
		analogWrite(myPin, abs(mySpeed));
	}

	void stop() {
		update(0);
	}
};
Motor motors[] = {Motor(mot1, mot1Dir), Motor(mot2, mot2Dir), Motor(mot3, mot3Dir), Motor(mot4, mot4Dir)};

void updateQuadDrive(int x, int y) {
	Serial.print("x: "); Serial.print(x); Serial.print("    y: "); Serial.println(y);
	motors[0].update(x);
	motors[1].update(x);
	motors[2].update(y);
	motors[3].update(y);
}

void smoothValue() {
// Smooths out engine movement
	currentMillis = millis();
	if ((currentMillis - previousMillis >= 1) and (smoothness > 0)) {
		hasUpdated = false;
		if (currentX != x) {
			currentX = x > currentX ? min(currentX + smoothness / 1000, x) : max(currentX - smoothness / 1000, x);
			hasUpdated = true;
		}
		if (currentY != y) {
			currentY = y > currentY ? min(currentY + smoothness / 1000, y) : max(currentY - smoothness / 1000, y);
			hasUpdated = true;
		}
		if (hasUpdated) {updateQuadDrive(currentX, currentY);}
		hasUpdated	= false;
		previousMillis = currentMillis;
	}
}

void getData(int bytes) {
	while (Wire.available()) {		// Loop through all received bytes
		char c = Wire.read();		// Receive byte as a character
		rawData += c;
	}

	// Decodes data
	int comma = rawData.indexOf(',');
	x = rawData.substring(0, comma).toInt();
	y = rawData.substring(comma + 1).toInt();
	rawData = "";
	if (smoothness <= 0) {
		updateQuadDrive(x, y);
	}
}


void loop() {
	smoothValue();
	// // Read current state of each FG pin
	// int currentState1 = digitalRead(MOTOR1_FG_PIN);
	// int currentState2 = digitalRead(MOTOR2_FG_PIN);
	// int currentState3 = digitalRead(MOTOR3_FG_PIN);
	// int currentState4 = digitalRead(MOTOR4_FG_PIN);

	// // Detect rising edge for each motor and count pulses
	// if (currentState1 == HIGH && lastState1 == LOW) pulseCount1++;
	// if (currentState2 == HIGH && lastState2 == LOW) pulseCount2++;
	// if (currentState3 == HIGH && lastState3 == LOW) pulseCount3++;
	// if (currentState4 == HIGH && lastState4 == LOW) pulseCount4++;

	// // Update last state
	// lastState1 = currentState1;
	// lastState2 = currentState2;
	// lastState3 = currentState3;
	// lastState4 = currentState4;

	// // Calculate RPM every rpmCalcInterval milliseconds
	// if (millis() - lastMillis >= rpmCalcInterval) {
	// 	lastMillis = millis();

	// 	// Calculate RPM
	// 	rpm1 = (pulseCount1 * 60.0) / (rpmCalcInterval / 1000.0);
	// 	rpm2 = (pulseCount2 * 60.0) / (rpmCalcInterval / 1000.0);
	// 	rpm3 = (pulseCount3 * 60.0) / (rpmCalcInterval / 1000.0);
	// 	rpm4 = (pulseCount4 * 60.0) / (rpmCalcInterval / 1000.0);

	// 	// Reset pulse counts for the next interval
	// 	pulseCount1 = 0;
	// 	pulseCount2 = 0;
	// 	pulseCount3 = 0;
	// 	pulseCount4 = 0;

	// }
}
