#include <Arduino.h>

#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED dfinitions
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     LED_BUILTIN // Reset pin # (or -1 if sharing Arduino reset pin)


// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_SCAN_TOPIC   "hitsScanner/scan"
#define AWS_IOT_SHADOW_UPDATE "$aws/things/Hits_Scanner/shadow/update"
#define AWS_IOT_SHADOW_UPDATE_DELTA "$aws/things/Hits_Scanner/shadow/update/delta"
#define AWS_IOT_SHADOW_UPDATE_ACCEPTED "$aws/things/Hits_Scanner/shadow/update/accepted"
#define AWS_IOT_SHADOW_UPDATE_REJECTED "$aws/things/Hits_Scanner/shadow/update/rejected"

// RFID Config
#define RST_PIN         LED_BUILTIN          // Configurable, see typical pin layout above
#define SS_PIN          5         // Configurable, see typical pin layout above

#define BUZZER_PIN      4

WiFiClientSecure net = WiFiClientSecure();


MQTTClient client = MQTTClient(256);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Init array that will store new NUID
byte nuidPICC[4];

unsigned long lastMillis = 0;


MFRC522 rfid(SS_PIN, RST_PIN);  // Create MFRC522 instance

void oledPrintMessage(String message) {
  display.clearDisplay();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.println(message);
  display.display();
}

void tone(byte pin, int freq) {
  ledcSetup(0, 2000, 8); // setup beeper
  ledcAttachPin(pin, 0); // attach beeper
  ledcWriteTone(0, freq); // play tone
}

void noTone() {
  tone(0, 0);
}

void beep() {
  tone(BUZZER_PIN, 2000);
  delay(300);
  noTone();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");
  oledPrintMessage("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
}

void connectAWS() {
  Serial.print("Connecting to AWS IOT");
  oledPrintMessage("Connecting to AWS...");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }

  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
//  client.subscribe(AWS_IOT_SHADOW_UPDATE);
//  client.subscribe(AWS_IOT_SHADOW_UPDATE_DELTA);
//  client.subscribe(AWS_IOT_SHADOW_UPDATE_ACCEPTED);
//  client.subscribe(AWS_IOT_SHADOW_UPDATE_REJECTED);
//  client.subscribe("$aws/things/Hits_Scanner/shadow/get/accepted");
//  client.subscribe("$aws/things/Hits_Scanner/shadow/get/rejected");
  client.subscribe("$aws/things/Hits_Scanner/shadow/update/documents");
//  client.subscribe("/hello");

  Serial.println("AWS IoT Connected!");

  oledPrintMessage("AWS IoT Connected!");
}

void publishScan(String uid) {

  Serial.println(uid);
  StaticJsonDocument<200> doc;
  doc["uid"] = uid;
  doc["time"] = millis();
  char jsonBuffer[512];

  serializeJson(doc, jsonBuffer); // print to client

//  client.publish("$aws/things/Hits_Scanner/shadow/get", "");
  client.publish(AWS_IOT_SCAN_TOPIC, jsonBuffer);
  beep();
  oledPrintMessage("Scanned: " + uid);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("incoming message...");
//  Serial.println("incoming: " + topic + " - " + payload);

//  StaticJsonDocument<200> doc;
//  deserializeJson(doc, payload);
//  const char* message = doc["message"];
}

String getUIDString(byte *buffer, byte bufferSize) {
  String uidString = "";

  for (byte i = 0; i < bufferSize; i++) {
    uidString += buffer[i] < 0x10 ? " 0" : " ";
    uidString += buffer[i];
  }

  return uidString;
}

void setup() {
  Serial.begin(9600);
  // Init SPI bus
  SPI.begin();

  // Init OLED screen
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  oledPrintMessage("Initialising...");

  // Init rfid reader
  rfid.PCD_Init();
  delay(4);        // Optional delay. Some board do need more time after init to be ready, see Readme
  rfid.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

  connectWiFi();

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  connectAWS();
}

void loop() {
  // Maintain a connection to the server
  client.loop();
  delay(500);

  if (!client.connected()) {
    Serial.println(F("Lost connection, reconnecting..."));
    connectAWS();
  }

  // publish a message roughly every second.
  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
    client.publish("/hello", "world");
  }

	// Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
	if ( ! rfid.PICC_IsNewCardPresent()) {
		return;
	}

	// Select one of the cards
	if ( ! rfid.PICC_ReadCardSerial()) {
		return;
	}

  /* Continue if successful card read... */

  //  Publish card read event
  publishScan(getUIDString(rfid.uid.uidByte, rfid.uid.size));

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();

  delay(1000);
}