#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>

// --- Bell 103 Standard Frequencies (300 Baud) ---
#define TONE_MARK   1270  // Frequency for Binary 1 (Hz)
#define TONE_SPACE  1070  // Frequency for Binary 0 (Hz)

const int BUZZER_PIN = 18;  // Piezo Buzzer Pin
const int PWM_CHANNEL = 0;  // LEDC PWM Channel
const int PWM_RES = 8;      // 8-bit resolution

// Modem State Variables
bool dataMode = false;
String cmdBuffer = "";
String host = "";
uint16_t port = 80; // Default HTTP port

// WiFi State Machine
enum WiFiSetupState {
  STATE_NORMAL,
  STATE_GET_SSID,
  STATE_GET_PASS
};
WiFiSetupState wifiState = STATE_NORMAL;
String tempSSID = "";
String tempPass = "";

WiFiClient client;

// --- SPIFFS Helper Functions ---
void saveCredentials(String ssid, String pass) {
  File ssidFile = SPIFFS.open("/ssid.txt", FILE_WRITE);
  if (ssidFile) {
    ssidFile.print(ssid);
    ssidFile.close();
  }
  File passFile = SPIFFS.open("/pass.txt", FILE_WRITE);
  if (passFile) {
    passFile.print(pass);
    passFile.close();
  }
  Serial.println("\r\nCredentials saved to SPIFFS.");
}

bool loadCredentials(String &ssid, String &pass) {
  if (!SPIFFS.exists("/ssid.txt") || !SPIFFS.exists("/pass.txt")) {
    return false;
  }
  
  File ssidFile = SPIFFS.open("/ssid.txt", FILE_READ);
  if (ssidFile) {
    ssid = ssidFile.readString();
    ssid.trim();
    ssidFile.close();
  }
  
  File passFile = SPIFFS.open("/pass.txt", FILE_READ);
  if (passFile) {
    pass = passFile.readString();
    pass.trim();
    passFile.close();
  }
  return (ssid.length() > 0);
}

void connectToWiFi(String ssid, String pass) {
  Serial.print("\r\nConnecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\r\nCONNECTED. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\r\nCONNECTION FAILED.");
  }
}

// --- Audio Transmitter (Modulator) ---
void sendBuzzerBit(bool bit) {
  unsigned int freq = bit ? TONE_MARK : TONE_SPACE;
  ledcWriteTone(PWM_CHANNEL, freq);
  delayMicroseconds(3333); // ~300 Baud timing
}

void transmitByte(char c) {
  // 1. Start Bit (Always 0 / Space tone)
  sendBuzzerBit(0);
  
  // 2. 8 Data Bits (LSB first)
  for (int i = 0; i < 8; i++) {
    bool bitValue = (c >> i) & 0x01;
    sendBuzzerBit(bitValue);
  }
  
  // 3. Stop Bit (Always 1 / Mark tone)
  sendBuzzerBit(1);
  
  // Turn off sound between pulses
  ledcWrite(PWM_CHANNEL, 0); 
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed! Formatting...");
  }

  // Initialize Buzzer PWM
  ledcSetup(PWM_CHANNEL, TONE_MARK, PWM_RES);
  ledcAttachPin(BUZZER_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0); 

  Serial.println("\r\n========================================");
  Serial.println("ESP32 HARDWARE MODEM READY.");
  Serial.println("Commands: P <port> | S <host> | W | NW | SW");
  Serial.println("========================================");
  Serial.println("OK");

  // Auto-connect on boot if credentials exist
  String savedSSID, savedPass;
  if (loadCredentials(savedSSID, savedPass)) {
    connectToWiFi(savedSSID, savedPass);
  } else {
    Serial.println("No saved WiFi credentials. Use 'W' to configure.");
  }
}

// --- Main Loop ---
void loop() {
  // 1. Handle outgoing Serial Data / Command Mode
  if (Serial.available()) {
    char inChar = Serial.read();

    if (wifiState == STATE_GET_SSID) {
      // Catch raw setup inputs without echoing command strings
      if (inChar == '\r' || inChar == '\n') {
        tempSSID.trim();
        if (tempSSID.length() > 0) {
          Serial.println("\r\nENTER PASSWORD:");
          wifiState = STATE_GET_PASS;
        }
      } else {
        Serial.print(inChar);
        tempSSID += inChar;
      }
    } 
    else if (wifiState == STATE_GET_PASS) {
      if (inChar == '\r' || inChar == '\n') {
        tempPass.trim();
        saveCredentials(tempSSID, tempPass);
        connectToWiFi(tempSSID, tempPass);
        wifiState = STATE_NORMAL;
        Serial.println("OK");
      } else {
        Serial.print("*"); // Mask password entry
        tempPass += inChar;
      }
    } 
    else if (!dataMode) {
      // --- COMMAND MODE ---
      Serial.write(inChar); // Echo back to terminal
      
      if (inChar == '\r' || inChar == '\n') {
        cmdBuffer.trim();
        
        if (cmdBuffer.startsWith("P ")) {
          // Set Port
          String portStr = cmdBuffer.substring(2);
          port = portStr.toInt();
          Serial.print("\r\nPORT SET TO: ");
          Serial.println(port);
          Serial.println("OK");
        } 
        else if (cmdBuffer.startsWith("S ")) {
          // Set host and try connecting
          host = cmdBuffer.substring(2);
          host.trim();
          
          if (WiFi.status() == WL_CONNECTED) {
            Serial.print("\r\nCONNECTING TO: ");
            Serial.print(host);
            Serial.print(":");
            Serial.println(port);
            
            // Retro Dial Sounds
            for(int i = 0; i < 4; i++) {
              ledcWriteTone(PWM_CHANNEL, 800 + (i * 200)); 
              delay(80);
            }
            ledcWrite(PWM_CHANNEL, 0);
            
            if (client.connect(host.c_str(), port)) {
              Serial.println("CONNECT 300 (DATA BRIDGE ACTIVE)");
              dataMode = true;
            } else {
              Serial.println("CONNECTION FAILED");
            }
          } else {
            // Local Offline Mode (No WiFi)
            Serial.println("\r\nOFFLINE MODE: NO WIFI. ENAMELED LOCAL ECHO.");
            dataMode = true;
          }
        } 
        else if (cmdBuffer == "W") {
          // Initiate Interactive WiFi Setup
          Serial.println("\r\nENTER WIFI SSID:");
          tempSSID = "";
          tempPass = "";
          wifiState = STATE_GET_SSID;
        } 
        else if (cmdBuffer == "NW") {
          // Disable WiFi
          WiFi.disconnect(true);
          Serial.println("\r\nWIFI DISABLED.");
          Serial.println("OK");
        } 
        else if (cmdBuffer == "SW") {
          // Restart WiFi
          String savedSSID, savedPass;
          if (loadCredentials(savedSSID, savedPass)) {
            connectToWiFi(savedSSID, savedPass);
          } else {
            Serial.println("\r\nNO CREDENTIALS FOUND. USE 'W' COMMAND.");
          }
          Serial.println("OK");
        }
        else if (cmdBuffer == "AT") {
          Serial.println("\r\nOK");
        }
        else if (cmdBuffer == "ATH") {
          if (client.connected()) {
            client.stop();
          }
          dataMode = false;
          Serial.println("\r\nDISCONNECTED / OK");
        }
        
        cmdBuffer = "";
      } else if (inChar != '\b') {
        cmdBuffer += inChar;
      }
    } 
    else {
      // --- DATA MODE (Online/Offline) ---
      // 1. Send audio burst
      transmitByte(inChar);
      
      // 2. If online, write to remote server socket
      if (client.connected()) {
        client.write(inChar);
      }
      
      // Keep a local escape hook "+++" to manually drop back to command mode
      if (inChar == '+') {
        static uint8_t plusCount = 0;
        plusCount++;
        if (plusCount >= 3) {
          delay(500); // Guard time
          dataMode = false;
          if (client.connected()) client.stop();
          Serial.println("\r\nNO CARRIER / OK");
          plusCount = 0;
        }
      }
    }
  }

  // 2. Handle incoming Network Data (If in Data Mode and connected to Server)
  if (dataMode && client.connected() && client.available()) {
    char outChar = client.read();
    Serial.write(outChar); // Output response to terminal screen
  }
}
