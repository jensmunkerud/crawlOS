#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// UUIDs for the service and characteristics (make sure these match exactly with the RP2040)
#define SERVICE_UUID "ceeeeeee-c666-499f-b917-352312f159c5"
#define CHARACTERISTIC_UUID_X "aaaaaaaa-d2a0-44c8-a271-69ef24094b01"
#define CHARACTERISTIC_UUID_Y "bbbbbbbb-f0a9-4623-b503-ee7804fca301"

BLEClient* pClient;
BLERemoteCharacteristic* xRemoteCharacteristic;
BLERemoteCharacteristic* yRemoteCharacteristic;
bool doConnect = false;
bool connected = false;
BLEAdvertisedDevice* myDevice;

class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
	void onResult(BLEAdvertisedDevice advertisedDevice) {
		Serial.print("BLE Advertised Device found: ");
		Serial.println(advertisedDevice.toString().c_str());

		// Check if the advertised device has the service UUID we're looking for
		if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
			Serial.println("Found a matching device, stopping scan and connecting...");
			BLEDevice::getScan()->stop();
			myDevice = new BLEAdvertisedDevice(advertisedDevice);
			doConnect = true;
		}
	}
};

void setupBLEClient() {
	// Initialize BLE and create a client
	BLEDevice::init("ESP32-Client");
	pClient = BLEDevice::createClient();
	
	// Set up a callback for devices detected during the scan
	BLEScan* pScan = BLEDevice::getScan();
	pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
	pScan->setActiveScan(true);
	pScan->start(100, false);
}

bool connectToServer() {
	if (pClient == nullptr || myDevice == nullptr) {
		Serial.println("Client or device is not initialized properly.");
		return false;
	}

	// Attempt to connect to the advertised device
	Serial.println("Attempting to connect...");
	pClient->connect(myDevice);

	if (pClient->isConnected()) {
		Serial.println("Connected to the server!");
		// Discover the service by its UUID
		BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
		if (pRemoteService == nullptr) {
			Serial.print("Failed to find the service UUID: ");
			Serial.println(SERVICE_UUID);
			pClient->disconnect();
			return false;
		}

		// Get characteristics for X and Y
		xRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID_X));
		yRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID_Y));


		if (xRemoteCharacteristic == nullptr || yRemoteCharacteristic == nullptr) {
			Serial.println("Failed to find characteristics.");
			pClient->disconnect();
			return false;
		}

		connected = true;
		return true;
	}

	Serial.println("Failed to connect to the device.");
	return false;
}

void writeJoystickData() {
	int16_t x = map(analogRead(2), 0, 4095, 0, 255);
	int16_t y = map(analogRead(15), 0, 4095, 0, 255);

	if (connected) {
		// Write X and Y values
		xRemoteCharacteristic->writeValue(x, sizeof(int16_t));
		yRemoteCharacteristic->writeValue(y, sizeof(int16_t));

		Serial.print("Sent X: ");
		Serial.print(x);
		Serial.print(" Sent Y: ");
		Serial.println(y);
	}
}


void setup() {
	Serial.begin(9600);
	setupBLEClient();
}

void loop() {
	if (doConnect) {
		if (connectToServer()) {
			Serial.println("Successfully connected and discovered characteristics.");
		} else {
			Serial.println("Failed to connect. Restarting scan...");
			BLEDevice::getScan()->start(5, false);
		}
		doConnect = false;
	}

	if (connected && pClient->isConnected()) {
		writeJoystickData();
	}
	
	if (not pClient->isConnected()) {
		// Restarts scan after lost connection
		Serial.println("Lost connection, starting scan..");
		BLEDevice::getScan()->start(100, false);
	}
}
