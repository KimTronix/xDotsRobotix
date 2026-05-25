/*
 * xDots Robotic Arm - ESP32 Wi-Fi to Serial Bridge (ZERO-DEPENDENCY)
 * ------------------------------------------------------------------
 * This firmware configures your ESP32 as a Wi-Fi Access Point (AP)
 * and runs a self-contained WebSocket Server on Port 81.
 *
 * This version uses ONLY built-in libraries (WiFi.h, mbedtls, base64) 
 * so it will compile immediately out-of-the-box in your Arduino IDE 
 * WITHOUT needing to download any external library packages!
 *
 * ESP32 Wi-Fi Credentials:
 *   SSID:     xDotsArm_WiFi
 *   Password: password123 (Change as desired)
 *   IP:       192.168.4.1 (Static default)
 *   Port:     81
 *
 * Connections:
 *   ESP32 GND  <--->  Arduino Mega GND (Common Ground is CRITICAL!)
 *   ESP32 TX2 (GPIO 17) <---> Arduino Mega RX1 (Pin 18)
 *   ESP32 RX2 (GPIO 16) <---> Arduino Mega TX1 (Pin 19) -- (Warning: Use level shifter or 1k/2k resistor divider!)
 */

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <mbedtls/sha1.h>
#include <base64.h>

// Wi-Fi Details (Enter your Home Wi-Fi here)
const char *ssid = "YOUR_WIFI_NAME";
const char *password = "YOUR_WIFI_PASSWORD";

// TCP Server on Port 81
WiFiServer server(81);
WiFiClient client;
bool handshakeDone = false;

#define BOARD_LED              2  // On-board blue LED on most ESP32 Dev Kits
#define MegaSerial Serial2
#define MEGA_BAUD 115200

// Performs the WebSocket handshake with the React Native client
void doHandshake(WiFiClient& client) {
  String req = "";
  unsigned long timeout = millis();
  
  // Read HTTP headers
  while (client.connected() && (millis() - timeout < 1000)) {
    if (client.available()) {
      char c = client.read();
      req += c;
      if (req.endsWith("\r\n\r\n")) break;
    }
  }
  
  // Find Sec-WebSocket-Key header
  int keyIndex = req.indexOf("Sec-WebSocket-Key: ");
  if (keyIndex != -1) {
    int keyStart = keyIndex + 19;
    int keyEnd = req.indexOf("\r\n", keyStart);
    String key = req.substring(keyStart, keyEnd);
    key.trim();
    
    // Concatenate key with WebSocket Guid magic string
    String acceptKey = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    // Compute SHA-1 hash
    uint8_t sha1Hash[20];
    mbedtls_sha1_context ctx;
    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts(&ctx);
    mbedtls_sha1_update(&ctx, (const unsigned char*)acceptKey.c_str(), acceptKey.length());
    mbedtls_sha1_finish(&ctx, sha1Hash);
    mbedtls_sha1_free(&ctx);
    
    // Base64 encode the SHA-1 digest
    String base64Hash = base64::encode(sha1Hash, 20);
    base64Hash.trim();
    
    // Send standard upgrade response headers
    client.print("HTTP/1.1 101 Switching Protocols\r\n");
    client.print("Upgrade: websocket\r\n");
    client.print("Connection: Upgrade\r\n");
    client.print("Sec-WebSocket-Accept: " + base64Hash + "\r\n\r\n");
    
    handshakeDone = true;
    digitalWrite(BOARD_LED, HIGH); // Light up LED to signal active WS session
    Serial.println("[WIFI BRIDGE] WebSocket Handshake successful. Client connected!");
  } else {
    // If not a WebSocket handshake, send a generic HTTP ok
    client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello from xDots ESP32 Wi-Fi Bridge.");
    client.stop();
  }
}

// Parses and decodes WebSocket frames sent from the mobile app
void handleClientData(WiFiClient& client) {
  if (client.available() >= 2) {
    uint8_t header1 = client.read();
    uint8_t header2 = client.read();
    
    // Extract opcode and masking bit
    uint8_t opcode = header1 & 0x0F;
    bool isMasked = (header2 & 0x80) != 0;
    uint32_t payloadLen = header2 & 0x7F;
    
    // Check if client requested connection close
    if (opcode == 0x08) {
      Serial.println("[WIFI BRIDGE] Connection close frame received.");
      client.stop();
      return;
    }
    
    // Resolve extended 16-bit payload length if applicable
    if (payloadLen == 126) {
      if (client.available() >= 2) {
        uint8_t len1 = client.read();
        uint8_t len2 = client.read();
        payloadLen = (len1 << 8) | len2;
      }
    }
    
    // Read the 4-byte masking key
    uint8_t mask[4] = {0};
    if (isMasked) {
      unsigned long timeout = millis();
      while (client.available() < 4 && (millis() - timeout < 500));
      client.readBytes(mask, 4);
    }
    
    // Read the raw masked payload data
    if (payloadLen > 0) {
      unsigned long timeout = millis();
      while (client.available() < payloadLen && (millis() - timeout < 500));
      
      uint8_t payload[payloadLen];
      client.readBytes(payload, payloadLen);
      
      // Toggle onboard LED off during processing (provides visual rx indicator)
      digitalWrite(BOARD_LED, LOW);
      
      // Unmask and write bytes to the Mega
      for (uint32_t i = 0; i < payloadLen; i++) {
        if (isMasked) {
          payload[i] ^= mask[i % 4];
        }
        
        uint8_t cmd = payload[i];
        
        // Send byte directly to Arduino Mega Serial interface
        MegaSerial.write(cmd);
        
        // Print transaction to USB console
        Serial.printf("[WIFI->MEGA] Forwarded Byte: %d\n", cmd);
      }
      
      digitalWrite(BOARD_LED, HIGH);
    }
  }
}

void setup() {
  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, LOW);

  // USB serial debugging
  Serial.begin(115200);
  Serial.println("\n--- xDots ESP32 Wireless Wi-Fi Bridge (Zero-Dependency) ---");

  // UART connection to Arduino Mega at 115200 baud
  MegaSerial.begin(MEGA_BAUD, SERIAL_8N1, 16, 17);
  Serial.println("[SYSTEM] UART Serial2 initialized at 115200 baud.");

  // Connect to Home Wi-Fi
  Serial.print("[WIFI] Connecting to Wi-Fi SSID: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  IPAddress myIP = WiFi.localIP();
  Serial.print("[WIFI] Wi-Fi Connected! Local IP address: ");
  Serial.println(myIP);
  
  Serial.println("-----------------------------------------------------");
  Serial.println("TYPE THIS IP ADDRESS INTO THE SETTINGS OF THE APP!");
  Serial.println("-----------------------------------------------------");

  // Start TCP server
  server.begin();
  Serial.println("[SYSTEM] Server listening on Port 81.");

  // Startup success indicator
  for(int i=0; i<3; i++) {
    digitalWrite(BOARD_LED, HIGH); delay(100);
    digitalWrite(BOARD_LED, LOW); delay(100);
  }
}

void loop() {
  // Check for client disconnect or incoming connections
  if (!client || !client.connected()) {
    if (client) {
      client.stop();
      Serial.println("[WIFI BRIDGE] Client disconnected.");
      digitalWrite(BOARD_LED, LOW);
      handshakeDone = false;
    }
    client = server.available();
    if (client) {
      Serial.println("[WIFI BRIDGE] New mobile client joined. Starting handshake...");
    }
  }

  // Manage active client
  if (client && client.connected()) {
    if (!handshakeDone) {
      doHandshake(client);
    } else {
      handleClientData(client);
    }
  }

  // Read status strings from Arduino Mega and echo to USB monitor
  if (MegaSerial.available() > 0) {
    char incoming = MegaSerial.read();
    Serial.print("[MEGA] ");
    Serial.println(incoming);
  }
}
