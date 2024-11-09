#include <Wire.h>

// Sets pins
#define mot1		3	// Blue
#define mot1Dir		2	// White
#define mot2		5
#define mot2Dir		4
#define mot3		6
#define mot3Dir		7
#define mot4		9
#define mot4Dir		8

// REDEFINE THESE
#define pistonInnerExt	10
#define pistonInnerRetr	11
#define pistonOuterExt	12
#define pistonOuterRetr	13

#define mot1RPM A0	// Yellow
#define mot2RPM A1
#define mot3RPM A2
#define mot4RPM A3

#define slaveID		1

// Variables to store pulse counts and last state for each motor
unsigned long pulseCount1 = 0;
unsigned long pulseCount2 = 0;
unsigned long pulseCount3 = 0;
unsigned long pulseCount4 = 0;
int lastState1 = LOW;
int lastState2 = LOW;
int lastState3 = LOW;
int lastState4 = LOW;
unsigned long lastMillis = 0; // Last time RPM was calculated
const unsigned long rpmCalcInterval = 1000; // Interval to calculate RPM (in ms)
float rpm1 = 0;
float rpm2 = 0;
float rpm3 = 0;
float rpm4 = 0;

// PARAMETERS
float smoothness = 255;		// units / sec		(disable = 0)
float currentX = 0;
float currentY = 0;
bool hasUpdated;
unsigned long currentMillis;
unsigned long previousMillis = 0;

int x, y;
int smooth = 1;
int exOuter = 0;
int exInner = 0;
int comma1, comma2, comma3, comma4;
String rawData;

void sendRPM() {
	Wire.write(int(rpm1));
	Wire.write(int(rpm2));
	Wire.write(int(rpm3));
	Wire.write(int(rpm4));
}

void setup() {
	// Starts I2C as SLAVE
	Wire.begin(slaveID);
	Wire.setTimeout(3000);
	Wire.onReceive(getData);
	Wire.onRequest(sendRPM);
	Serial.begin(9600);
}

class Motor {
	public:
		int myPin;
		int myDirPin;
		Motor(int motPin, int dirPin, int rpmPin) {
			myPin = motPin;
			myDirPin = dirPin;
			pinMode(myPin, OUTPUT);	pinMode(myDirPin, OUTPUT); pinMode(rpmPin, INPUT);
		}

	void update(int speed) {
		if (speed > 0) {
			pinMode(myDirPin, OUTPUT);	// CW Rotation
		} else {
			pinMode(myDirPin, INPUT);	// CCW Rotation
		}
		analogWrite(myPin, abs(speed));
	}
};
Motor motors[] = {Motor(mot1, mot1Dir, mot1RPM), Motor(mot2, mot2Dir, mot2RPM), Motor(mot3, mot3Dir, mot3RPM), Motor(mot4, mot4Dir, mot4RPM)};

void updateQuadDrive(int x, int y) {
	// Serial.print("x: "); Serial.print(x); Serial.print("	y: "); Serial.println(y);
	motors[0].update(x);
	motors[1].update(y);
	motors[2].update(x);
	motors[3].update(y);
}

void smoothValue() {
// Smooths out engine movement
	currentMillis = millis();
	if ((currentMillis - previousMillis >= 10) and (smoothness > 0)) {
		hasUpdated = false;
		if (currentX != x) {
			currentX = x > currentX ? min(currentX + smoothness / 100, x) : max(currentX - smoothness / 100, x);
			hasUpdated = true;
		}
		if (currentY != y) {
			currentY = y > currentY ? min(currentY + smoothness / 100, y) : max(currentY - smoothness / 100, y);
			hasUpdated = true;
		}
		if (hasUpdated) {updateQuadDrive(currentX, currentY);}
		hasUpdated	= false;
		previousMillis = currentMillis;
	}
}

void getData(int bytes) {
	// Loop through all received bytes, receive byte as a character
	char rawData[32];
	int index = 0;
	while (Wire.available() && index < sizeof(rawData) - 1) {
		rawData[index++] = Wire.read();
	}
	rawData[index] = '\0';  // Null-terminate the string

	// Parse the data
	char *ptr = strtok(rawData, ",");
	x = atoi(ptr);
	ptr = strtok(NULL, ",");
	y = atoi(ptr);
	ptr = strtok(NULL, ",");
	smooth = atoi(ptr);
	ptr = strtok(NULL, ",");
	exInner = atoi(ptr);
	ptr = strtok(NULL, ",");
	exOuter = atoi(ptr);

	// while (Wire.available()) {
	// 	char c = Wire.read();
	// 	rawData += c;
	// }
	// // Decodes data
	// comma1 = 	rawData.indexOf(',');
	// comma2 = 	rawData.indexOf(',', comma1 + 1);
	// comma3 = 	rawData.indexOf(',', comma2 + 1);
	// comma4 = 	rawData.indexOf(',', comma3 + 1);
	// x = 		rawData.substring(0, comma1).toInt();
	// y = 		rawData.substring(comma1 + 1).toInt();
	// smooth = 	rawData.substring(comma2 + 1, comma3).toInt();
	// exInner = 	rawData.substring(comma3 + 1, comma4).toInt();
	// exOuter = 	rawData.substring(comma4 + 1).toInt();
	// rawData = "";


	movePiston(true, exInner);
	movePiston(false, exOuter);
	if (smooth == 0) {
		// Instantly stop motors
		currentX = 0; currentY = 0;
		updateQuadDrive(x, y);
	}
}


void movePiston(bool Inner, int val) {
	Serial.print("wrote "); Serial.print(val > 0 ? "HIGH" : "LOW"); Serial.print(" to "); Serial.println(Inner ? "pistonInnerExt" : "pistonOuterExt");
	digitalWrite(Inner ? pistonInnerExt : pistonOuterExt,   val > 0 ? HIGH : LOW);
	digitalWrite(Inner ? pistonInnerRetr : pistonOuterRetr, val < 0 ? HIGH : LOW);
	if (Inner and val == 0) {
		digitalWrite(pistonInnerExt, LOW);
	}
}

void countRPM() {
	int currentState1 = digitalRead(mot1RPM);
	int currentState2 = digitalRead(mot2RPM);
	int currentState3 = digitalRead(mot3RPM);
	int currentState4 = digitalRead(mot4RPM);

	// Detect rising edge for each motor and count pulses
	if (currentState1 == HIGH && lastState1 == LOW) pulseCount1++;
	if (currentState2 == HIGH && lastState2 == LOW) pulseCount2++;
	if (currentState3 == HIGH && lastState3 == LOW) pulseCount3++;
	if (currentState4 == HIGH && lastState4 == LOW) pulseCount4++;

	// Update last state
	lastState1 = currentState1;
	lastState2 = currentState2;
	lastState3 = currentState3;
	lastState4 = currentState4;

	// Calculate RPM every rpmCalcInterval milliseconds
	if (millis() - lastMillis >= rpmCalcInterval) {
		lastMillis = millis();

		// Calculate RPM
		rpm1 = (pulseCount1 * 60.0) / (rpmCalcInterval * 0.56925);
		rpm2 = (pulseCount2 * 60.0) / (rpmCalcInterval * 0.56925);
		rpm3 = (pulseCount3 * 60.0) / (rpmCalcInterval * 0.56925);
		rpm4 = (pulseCount4 * 60.0) / (rpmCalcInterval * 0.56925);
		// Reset pulse counts for the next interval
		pulseCount1 = 0;
		pulseCount2 = 0;
		pulseCount3 = 0;
		pulseCount4 = 0;
		// Serial.print("mot1: "); Serial.print(rpm1); Serial.print("   mot2: "); Serial.println(rpm2);
	}
}


void loop() {
	if (smooth == 1) {smoothValue();}
	// countRPM();
}

