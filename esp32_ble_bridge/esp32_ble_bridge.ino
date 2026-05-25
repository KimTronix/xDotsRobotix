/*
 * xDots Robotic Arm - ESP32 BLE to Serial Bridge
 * ----------------------------------------------------
 * This firmware runs on an ESP32 to act as a wireless BLE bridge
 * for the xDots mobile React Native application. 
 *
 * It emulates an HM-10 BLE module (Service FFE0, Characteristic FFE1)
 * so that the React Native app can connect to "xDotsArm" out-of-the-box
 * without changing a single line of React Native or Arduino Mega code.
 *
 * Connections:
 *   ESP32 GND  <--->  Arduino Mega GND (Common Ground is CRITICAL!)
 *   ESP32 TX2 (GPIO 17) <---> Arduino Mega RX1 (Pin 18)  -- Direct connection is safe
 *   ESP32 RX2 (GPIO 16) <---> Arduino Mega TX1 (Pin 19)  -- WARNING: MUST USE VOLTAGE DIVIDER/LEVEL SHIFTER!
 *                                                          (Mega TX1 is 5V, ESP32 RX2 is 3.3V max!)
 *
 * Voltage Divider for Mega TX1 -> ESP32 RX2:
 *   [Mega Pin 19] ---> [ 1k Ohm Resistor ] ---> [ ESP32 GPIO 16 ] ---> [ 2k Ohm Resistor ] ---> [ GND ]
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// BLE UUIDs for HM-10 emulation (matches react-native-ble-plx client)
#define SERVICE_UUID           "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"

#define BOARD_LED              2  // On-board blue LED on most ESP32 Dev Kits

// Using Serial2 (GPIO 16 RX, GPIO 17 TX) to leave Serial0 (USB) free for debugging.
#define MegaSerial Serial2
#define MEGA_BAUD 115200

bool deviceConnected = false;

// Callbacks for server connection states
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(BOARD_LED, HIGH); // Turn LED solid on when connected
      Serial.println("[NEXUS ESP32] Mobile app connected via BLE.");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(BOARD_LED, LOW); // Turn LED off when disconnected
      Serial.println("[NEXUS ESP32] Mobile app disconnected. Re-advertising...");
      pServer->startAdvertising(); // Restart advertising to allow re-connections
    }
};

// Callbacks for data receipt on FFE1 Characteristic
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        // Blink LED to indicate active wireless RX
        digitalWrite(BOARD_LED, LOW);
        
        for (int i = 0; i < rxValue.length(); i++) {
          uint8_t byteReceived = (uint8_t)rxValue[i];
          
          // Forward the raw byte to Mega Serial1
          MegaSerial.write(byteReceived);
          
          // Also print to USB Serial for developer debugging
          Serial.print("[BLE->MEGA] Sent Byte: ");
          Serial.println(byteReceived);
        }
        
        digitalWrite(BOARD_LED, HIGH);
      }
    }
};

void setup() {
  // Initialize on-board LED for status
  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, LOW);

  // Initialize USB Serial (Debugging)
  Serial.begin(115200);
  Serial.println("--- xDots ESP32 Wireless BLE Bridge ---");

  // Initialize Serial2 (ESP32 Pin 16 = RX, Pin 17 = TX) to Arduino Mega Serial1 (Pin 18/19)
  MegaSerial.begin(MEGA_BAUD, SERIAL_8N1, 16, 17);
  Serial.println("[SYSTEM] UART Serial2 initialized at 115200 baud.");

  // Initialize BLE Device
  BLEDevice::init("xDotsArm"); // This name must match exactly what the React Native app scans for

  // Create BLE Server and bind callbacks
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create Characteristic with WRITE properties
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_WRITE | 
                                         BLECharacteristic::PROPERTY_WRITE_NR
                                       );

  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Start the BLE Service
  pService->start();

  // Start advertising so mobile app can see the device
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Help iOS latency connections
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("[SYSTEM] BLE Advertising active. Awaiting mobile connection...");
  
  // Blink 3 times to signal setup success
  for(int i=0; i<3; i++) {
    digitalWrite(BOARD_LED, HIGH); delay(100);
    digitalWrite(BOARD_LED, LOW); delay(100);
  }
}

void loop() {
  // We can read response states from the Arduino Mega (e.g. Servo updates, battery status) 
  // and forward them back to the BLE client if needed in the future.
  if (MegaSerial.available() > 0) {
    // Read from Mega, print to USB Serial for debugging
    char incoming = MegaSerial.read();
    Serial.print("[MEGA->BLE] Got response char: ");
    Serial.println(incoming);
  }
  delay(10);
}
