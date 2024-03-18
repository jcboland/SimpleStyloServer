#include <Preferences.h>
//#include <CRC32.h>

#include <WiFi.h>
#define TXD2 17
#define RXD2 18
#define INDICATORLED 8

#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "360d5a37-0ba0-41eb-b630-447fe22ddebc"
#define CHARACTERISTIC_SSID_UUID "05767dd7-0e3b-46a0-a3c3-507f4c23f209"
#define CHARACTERISTIC_PWD_UUID "03051fc7-67df-4d3d-aad6-5149ea50adbc"
#define CHARACTERISTIC_IP_UUID "7879b3ba-a2fe-4854-bec5-0ac3ceb4328e"

BLECharacteristic *pCharacteristicSSID;
BLECharacteristic *pCharacteristicPWD;
BLECharacteristic *pCharacteristicIP;
BLEService *pService;

Preferences preferences;
const char* SSID_PREF = "SSID_PREF";
const char* PWD_PREF = "PWD_PREF";
const char* IP_PREF = "IP_PREF";

int STATUSTIMEOUT = 500;
long lastStatusCheck = millis();
boolean indicatorOn = false;


int BUFFERSIZE = 1000;
String cmdBuffer[1000];
int availBuff = BUFFERSIZE;
int head = 0;
int tail = 0;
boolean sendCMDFlag = true;

String ssid = "Woodforest";
String password = "xfinity01";
String ipAddress = "";

bool logCOMM = false;

//const char* ssid     = "fourdognight";
//const char* password = "bored2018cities";
//const char* ssid     = "Door To Nowhere";
//const char* password = "goingknowhere";

WiFiServer server(1235);

//CRC32 crc;

void savePreference(const char* key, String value) {
  preferences.putString(key, String(value));
}

void loadPreferences() {
  preferences.begin("credentials", false);
  ssid = preferences.getString("ssid", "No SSID Preference Found").c_str();
  password = preferences.getString("password", "No Password Preference Found").c_str();
  ipAddress = preferences.getString("ipaddress", "No IP Preference Found").c_str();
  Serial.print("Loaded SSID: ");
  Serial.println(ssid);
  Serial.print("Loaded PWD: ");
  Serial.println(password);
  Serial.print("Loaded IP: ");
  Serial.println(ipAddress);
  preferences.end();
}

class MyServerCallbacks: public BLEServerCallbacks{
    void onDisconnect(BLEServer* pServer){
        BLEAdvertising *pAdvertising = pServer->getAdvertising();
        pAdvertising->start();
        //pServer->startAdvertising(); // Restart advertising
    }
};


class BLECallbacks: public BLECharacteristicCallbacks {

    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      BLEUUID bleUUID = pCharacteristic->getUUID();
      std::string targetUUIDStr = bleUUID.toString();

      if (targetUUIDStr == CHARACTERISTIC_SSID_UUID) {
        Serial.println("Writing to SSID UUID: ");
        preferences.begin("settings", false);
        preferences.putString(SSID_PREF, value.c_str());
        Serial.print("Saving SSID: ");
        Serial.println(preferences.getString(SSID_PREF, "No SSID Preference Found"));
        preferences.end();
      }
      else if (targetUUIDStr == CHARACTERISTIC_PWD_UUID) {
        Serial.println("Writing to PWD UUID: ");
        preferences.begin("settings", false);
        preferences.putString(PWD_PREF, value.c_str());
        Serial.print("Saving Password: ");
        Serial.println(preferences.getString(PWD_PREF, "No PWD Preference Found"));
        preferences.end();
        Serial.println("Rebooting...");
        ESP.restart();
      }
      else if (targetUUIDStr == CHARACTERISTIC_IP_UUID) {
        Serial.println("Writing to IP UUID: ");

      }
      else {
        Serial.println("Attempted to write to characteristic NOT FOUND");
      }
    }
};


void loadWIFI() {
  //  preferences.begin("settings", false);
  //  preferences.putString(SSID_PREF, "Door To Nowhere");
  //  preferences.putString(PWD_PREF, "goingknowhere");
  //  preferences.putString(IP_PREF, "10.0.10.191");
  //  preferences.end();

  preferences.begin("settings", false);
  Serial.print("Loaded SSID: ");
  Serial.println(preferences.getString(SSID_PREF, "No SSID Preference Found"));
  ssid = preferences.getString(SSID_PREF, "Default");
  Serial.print("Loaded PWD: ");
  Serial.println(preferences.getString(PWD_PREF, "No Password Preference Found"));
  password = preferences.getString(PWD_PREF, "Default");
  Serial.print("Loaded IP: ");
  Serial.println(preferences.getString(IP_PREF, "No IP Address Preference Found"));
  ipAddress = preferences.getString(IP_PREF, "Default");
  preferences.end();

  //loadPreferences();
}

void startWIFI() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  //WiFi.begin(ssid.c_str(), password.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    displayDisconnected();
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  ipAddress = WiFi.localIP().toString();
  pCharacteristicIP->setValue(ipAddress.c_str());

  server.begin();
}

void setupSerial() {

  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); // 9600 Works well 

  while (!Serial2) {
    delay(10);
    Serial.println("Waiting on Serial 2...");
    while (!Serial) {}
    delay(10);
    Serial.println("Waiting on Serial ...");
  }
  delay(10);
}

void setupBLE() {
  BLEDevice::init("Stylo");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  pService = pServer->createService(SERVICE_UUID);

  pCharacteristicSSID = pService->createCharacteristic(
                          CHARACTERISTIC_SSID_UUID,
                          BLECharacteristic::PROPERTY_READ |
                          BLECharacteristic::PROPERTY_WRITE
                        );

  pCharacteristicPWD = pService->createCharacteristic(
                         CHARACTERISTIC_PWD_UUID,
                         BLECharacteristic::PROPERTY_READ |
                         BLECharacteristic::PROPERTY_WRITE
                       );

  pCharacteristicIP = pService->createCharacteristic(
                        CHARACTERISTIC_IP_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_WRITE
                      );

  // Set callbacks for the Characteristics
  pCharacteristicSSID->setCallbacks(new BLECallbacks());
  pCharacteristicPWD->setCallbacks(new BLECallbacks());
  pCharacteristicIP->setCallbacks(new BLECallbacks());

  pCharacteristicSSID->setValue("");
  pCharacteristicPWD->setValue("");
  pCharacteristicIP->setValue("");
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();

  printDeviceAddress();
}

void setup() {
  setupLED();
  setupSerial();
  loadWIFI();
  setupBLE();
  startWIFI();
}

void setupLED(){
  pinMode(INDICATORLED, OUTPUT);
  digitalWrite(INDICATORLED, HIGH);
}

void printDeviceAddress() {
  Serial.print("BLE Address: ");
  const uint8_t* point = esp_bt_dev_get_address();
  for (int i = 0; i < 6; i++) {
    char str[3];
    sprintf(str, "%02X", (int)point[i]);
    Serial.print(str);
    if (i < 5) {
      Serial.print(":");
    }
  }
}

void displayDisconnected(){
    
    // Update Indicator Light
    if((millis() - lastStatusCheck) > STATUSTIMEOUT){
      Serial.println("Flip");
      
      if(indicatorOn){
        digitalWrite(INDICATORLED, LOW);
        indicatorOn = false;
      }
      else{
        digitalWrite(INDICATORLED, HIGH);
        indicatorOn = true;
      }
      lastStatusCheck = millis(); 
    }
}

void loop() {
  
  WiFiClient client = server.available();   // listen for incoming clients

  ////////////////////////////////////////////////
  // Check if there is anything available from the Android Tablet
  if (client) {                             // if you get a client,
    Serial.println("New Client.");           // print a message out the serial port

    String currAndroidLine = "";                // make a String to hold incoming data from the android client
    String currBTTLine = "";   
    
    while (client.connected()) {            // loop while the client's connected
      
      //This Loop takes approximately 10ms
    
      ////////////
      // STEP 1 - Check for incoming messages from the tablet via wifi client
      ////////////
      // Check if the wifi client is available
      while (client.available()) {
        // Read a character
        char c = client.read();             // read a byte, then
        // Add it to the current line       
        currAndroidLine += c;
        // If it is the end of a command
        if (c == '\n') {      
          // Send currAndroidLine to the BTT
          Serial2.print(currAndroidLine);
          // Clear the currAndroidLine
          currAndroidLine = "";
        }
      }
      ////////////
      // STEP 2 - Check for incoming messages from the BTT
      ////////////
      while(Serial2.available() > 0){
        // Read the next available char
        char inChar = Serial2.read();
        // Add it to the running word
        currBTTLine += inChar;
        // Check if it is the ned of the line
        if(inChar == '\n'){
          // Send it to the Android
          client.print(currBTTLine);
          // Clear the current line
          currBTTLine = "";
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}
