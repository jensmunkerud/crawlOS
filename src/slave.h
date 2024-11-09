#include <SPI.h>

// Sets pins
#define mot1		3	// Blue
#define mot1Dir		2	// White
#define mot2		5
#define mot2Dir		4
#define mot3		6
#define mot3Dir		7
#define mot4		9
#define mot4Dir		8

#define pistonInnerExt	A4
#define pistonInnerRetr	A5
#define pistonOuterExt	A6
#define pistonOuterRetr	A7

#define mot1RPM A0	// Yellow
#define mot2RPM A1
#define mot3RPM A2
#define mot4RPM A3

// SPI Variables
const byte bufferSize = 32;      // Adjust size as needed for data length
volatile byte receivedIndex = 0;  // Index for buffer
volatile bool dataReceived = false;
char receivedData[bufferSize];    // Buffer to hold received data

// RPM Variables
int motRPM[] = {mot1RPM, mot2RPM, mot3RPM, mot4RPM};
byte rpmValues[4] = {8, 16, 32, 64};
int currentState[4] = {0, 0, 0, 0};
int lastState[4] = {0, 0, 0, 0};
int pulseCount[4] = {0, 0, 0, 0};
float rpm[4] = {0, 0, 0, 0};
unsigned long lastMillis = 0; // Last time RPM was calculated
const unsigned long rpmCalcInterval = 1000; // Interval to calculate RPM (in ms)
volatile int rpmIndex = 1;
volatile bool wantRPM = false;


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


void setup() {
	pinMode(MISO, OUTPUT);
	SPCR |= _BV(SPE); 		// Enable SPI in Slave mode
	SPI.attachInterrupt();
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

void getData(char *data) {
  // Parse the received comma-separated data
	char *token = strtok(data, ",");
	if (token != NULL) x = atoi(token);

	token = strtok(NULL, ",");
	if (token != NULL) y = atoi(token);

	token = strtok(NULL, ",");
	if (token != NULL) smooth = atoi(token);

	token = strtok(NULL, ",");
	if (token != NULL) exInner = atoi(token);

	token = strtok(NULL, ",");
	if (token != NULL) exOuter = atoi(token);
	Serial.print(x); Serial.print(", "); 
	Serial.print(y); Serial.print(", "); 
	Serial.print(smooth); Serial.print(", "); 
	Serial.print(exInner); Serial.print(", "); 
	Serial.println(exOuter);
	movePiston(true, exInner);
	movePiston(false, exOuter);
	if (smooth == 0) {
		// Instantly stop motors
		currentX = 0; currentY = 0;
		updateQuadDrive(x, y);
	}
}


void movePiston(bool Inner, int val) {
	// Serial.print("wrote "); Serial.print(val > 0 ? "HIGH" : "LOW"); Serial.print(" to "); Serial.println(Inner ? "pistonInnerExt" : "pistonOuterExt");
	digitalWrite(Inner ? pistonInnerExt : pistonOuterExt,   val > 0 ? HIGH : LOW);
	digitalWrite(Inner ? pistonInnerRetr : pistonOuterRetr, val < 0 ? HIGH : LOW);
	if (Inner and val == 0) {
		digitalWrite(pistonInnerExt, LOW);
	}
}


void countRPM() {
	// Detect rising edge for each motor and count pulses
	for (int i = 0; i < 4; i++) {
		currentState[i] = digitalRead(motRPM[i]); // motRPM[] contains the motor pins
		if (currentState[i] == HIGH && lastState[i] == LOW) {
			pulseCount[i]++;
		}
		lastState[i] = currentState[i];
	}
	// Calculate RPM every rpmCalcInterval milliseconds
	if (millis() - lastMillis >= rpmCalcInterval) {
		lastMillis = millis();
		// Calculate RPM for each motor
		for (int i = 0; i < 4; i++) {
			rpm[i] = min((pulseCount[i] * 60.0) / (rpmCalcInterval * 0.56925), 255);
			pulseCount[i] = 0; // Reset pulse count for next interval
		}
		// Serial.print("mot1: "); Serial.print(rpm[0]); Serial.print("   mot2: "); Serial.println(rpm[1]);
	}
}


void loop() {
	countRPM();
	if (smooth == 1) {smoothValue();}
	if (dataReceived) {
		dataReceived = false;
		getData(receivedData);
	}
	rpmValues[0] = analogRead(A2)*100;
}


ISR(SPI_STC_vect) {
	if (wantRPM) {
		SPDR = rpmValues[rpmIndex];
		rpmIndex++;
		if (rpmIndex >= 4) {
			rpmIndex = 1;	// Start rpmIndex at 1, since initial function call sets rpmValues[0]
			wantRPM = false;
		}
	} else {
		char receivedByte = SPDR;
		if (receivedByte == 'r') { // On the next 4 calls of this function, return RPM[i]
			wantRPM = true;
			SPDR = rpmValues[0];	// Sends initial rpmVal...
		}

		// Regular x, y, smooth, exInner, exOuter data received
		if (receivedIndex < bufferSize - 1) {
			receivedData[receivedIndex++] = receivedByte;
			if (receivedByte == '\n') {  // Newline marks end of data packet
				dataReceived = true;
				receivedData[receivedIndex] = '\0'; // Null-terminate the string
				receivedIndex = 0; // Reset for next transmission
			}
		} else {
		// Buffer overflow, reset buffer
		receivedIndex = 0;
		}
	}
}
