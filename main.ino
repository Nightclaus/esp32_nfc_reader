#define USER_SETUP_LOADED  // Prevent loading default setup
#include "User_Setup.h"
#include "image1.h"
#include "image2.h"
#include <TFT_eSPI.h>
#include <Wire.h> 
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// NFC I2C pinouts
#define SDA_PIN     21
#define SCL_PIN     22
#define PN532_IRQ   4   
#define PN532_RESET 2   

// TFT Display Pin Definitions (also defined in User_Setup.h)
#define TFT_CS    5
#define TFT_DC    33
#define TFT_RST   32
#define TFT_MOSI  23
#define TFT_SCLK  18
#define TFT_MISO  19
#define TFT_LED   16  // Optional, controls the backlight (connect to 3.3V if unused)

// Notes, No logs are not stored

///////////////////////////////////////////////////////////////////////////////////

// Create PN532 instance for I2C (IRQ, RESET)
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);  // Initialize for I2C communication // skipped PN532_IRQ, PN532_RESET

// TFT display initialization
TFT_eSPI tft = TFT_eSPI();  // TFT instance

///////////////////////////////////////////////////////////////////////////////////

bool hasWifi = false;

const char* ssid = "WiFi-52757A"; //"S&C Guest"; 
const char* password = "54035744"; //"buildingtomorrow";

const String apiKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Indvd3R1Y3V5bW9waHh5bWRuYndsIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDI3NzAwMjgsImV4cCI6MjA1ODM0NjAyOH0.5ognkN_N-1zBNk4Wo0kBdoEzi1vGK0FxND2y0vZRdHI";
const String serverUrl = "https://nfc-api-liart.vercel.app/api/getScraps?apiKey="+apiKey+"&serial=";

uint16_t resetCounter = 0; // If it become 20, 4 seconds would have passed
bool isShowingScore = false;

uint16_t pollingDelay = 200;
uint16_t millisecondsDelay = 2000;

///////////////////////////////////////////////////////////////////////////////////

void initDisplay() {
  // Initialize the display
  tft.init();
  tft.setRotation(1);  // Landscape mode

  // Log screen
  tft.fillScreen(TFT_BLACK);
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  
}

uint16_t timeoutCounter = 0;

void initWifi() {
  // Switch to Station mode

  /*
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);               // Reset saved WiFi
  delay(100);
  
  uint8_t customMac[] = {0x10, 0x9F, 0x41, 0xB6, 0x00, 0xEB};
  esp_wifi_set_mode(WIFI_MODE_STA); // Must be in station mode before setting MAC
  esp_wifi_set_mac(WIFI_IF_STA, customMac);
  */
  
  WiFi.begin(ssid, password);

  tft.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
    if (timeoutCounter++ >= 30) { // Waits 15 seconds
      tft.println("Timeout, Connection Failed");
      timeoutCounter=0;
      return;
    }
    
  }
  tft.print("\n  IP address: ");
  tft.println(WiFi.localIP());
  hasWifi = true;
  tft.println("  connected!");
}

void initNFC() {
  tft.println("Initializing NFC...");

  // Begin using the PN532 on I2C
  nfc.begin();

  // Obtain PN532 firmware version to verify communication
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    tft.println("Didn't find PN532 board");
    while (1);  // Halt if no board is found, Prob wrong switch or wrong wiring
  }

  tft.print("Found PN532 with firmware version: ");
  tft.println((versiondata >> 16) & 0xFF, HEX);

  // Configure the PN532 to read NFC tags
  if (nfc.SAMConfig()) {
    tft.println("NFC initialized successfully.");
  } else {
    tft.println("NFC initialization failed.");
  }

  delay(300);
}

///////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Start I2C on our defined pins
  Wire.begin(SDA_PIN, SCL_PIN);
  
  initDisplay(); // Defaults to log screen

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.println("Initialised Display");

  initNFC();  // Initialize NFC module using I2C

  initWifi();

  tft.println("Booting Complete!");
  delay(2000);

  displayTapImage();
}

///////////////////////////////////////////////////////////////////////////////////

void loop() {
  uint8_t uid[7];             // Buffer to store the UID (7 bytes max for MIFARE)
  uint8_t uidLength;          // Actual length of the UID
  bool success;

  tft.setTextSize(1);
  //tft.println("Waiting for NFC...");

  // Try to read a tag. This waits up to 1 second
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 1000);

  if (success) {
    // Debugging
    Serial.print("UID Length: ");
    Serial.println(uidLength);

    Serial.print("Tag UID: ");
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) Serial.print("0");
      Serial.print(uid[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    String serialNumber = "";
    for (int i = 0; i < uidLength; i++) {
      serialNumber += String(uid[i], HEX);
    }

    // Result
    if (hasWifi) {
      String stringScraps = getScrapsFromJSON(sendSerialToAPIwithResponse(serialNumber));
      delay(800);
      displayScraps(stringScraps);
    } else {
      displayScraps((char*)"No Wifi");
     }
    
    isShowingScore = true; // Init the timer for reseting

    delay(1500);  // Wait before next read
  } else {
    // Passive feedback
    Serial.println("No tag detected.");
  }

  delay(pollingDelay);  // Polling delay
  if (isShowingScore && resetCounter++ >= ((millisecondsDelay-1500)/pollingDelay)) { // Account for that delay
    resetCounter=0;
    isShowingScore = false;
    displayTapImage();
   }
}

///////////////////////////////////////////////////////////////////////////////////

void displayTapImage() {
    tft.fillScreen(TFT_WHITE);
    tft.drawBitmap(0, 0, image1, tft.width(), tft.height(), TFT_BLACK);
 }

void displayScraps(String message) {
  tft.fillScreen(TFT_WHITE);
  tft.drawBitmap(0, 0, image2, tft.width(), tft.height(), TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int textSize = 1;
  int lenOfMessage = message.length();

  if (lenOfMessage > 16) {
    textSize = 1;
  } else if (lenOfMessage <= 4) {
    textSize = 5;
  } else {
    textSize = 2;
  }

  tft.setTextSize(textSize);
  int16_t screenWidth = tft.width();
  int16_t screenHeight = tft.height();
  int16_t textPixelWidth = tft.textWidth(message);

  int16_t x = (screenWidth - textPixelWidth) / 2;
  int16_t y = (screenHeight - 64) / 2;

  tft.setCursor(x, y);
  tft.println(message);
}

///////////////////////////////////////////////////////////////////////////////////

String sendSerialToAPIwithResponse(String serialNum) {
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.print("Found Serial: ");
  tft.println(serialNum);
  tft.println("Resolving...");
  
  HTTPClient http;
  String fullUrl = String(serverUrl) + serialNum;

  unsigned long startTime = millis();  // Start timing

  http.begin(fullUrl);
  int httpCode = http.GET();

  unsigned long endTime = millis();    // End timing
  unsigned long responseTime = endTime - startTime;
  
  tft.print("HTTP Response Time: ");
  tft.print(responseTime);
  tft.println(" ms");

  if (httpCode > 0) {
    tft.print("HTTP Reponse: ");
    tft.println(httpCode);
    if (httpCode == 200) { // JSON Response
      tft.println("Recieved Successful Reponse");
    }
    if (httpCode == 201) { // JSON Response
      tft.println("Appending Entry, Created new entry");
    }
    if (httpCode == 500) { // JSON Response
      tft.println("Fatal Error.");
    }
    String response = http.getString();
    return response;
  } else {
    tft.println("HTTP request failed");
  }
  
  http.end();
  delay(2000);
}

String getScrapsFromJSON(String response) {
  tft.setTextSize(1);
  tft.println("API Response: " + response);

  /*
  int start = response.indexOf(":") + 1;
  int end = response.indexOf("}");
  
  String scrapStr = response.substring(start, end);
  int scraps = scrapStr.toInt();
  */

  StaticJsonDocument<200> doc; // 200 bytets of memory
  deserializeJson(doc, response);  // <- You need this line

  String scraps = doc["scraps"];

  tft.setTextSize(1);
  tft.print("You have " + scraps + " Scraps.");
  return String(scraps);
}

///////////////////////////////////////////////////////////////////////////////////

void screenTestCase() {
  tft.fillScreen(TFT_BLACK); // Fill background first
  
  // Text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 20);
  tft.println("Display Ready!");

  // Shapes
  tft.drawCircle(60, 100, 20, TFT_RED);
  tft.drawLine(20, 140, 100, 140, TFT_BLUE);
}
