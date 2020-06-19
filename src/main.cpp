#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "secrets.h"
#include "./Buzzer/Buzzer.h"
#include "./Screen/Screen.h"

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_SCAN_TOPIC   "hitsScanner/scan"
#define AWS_IOT_SHADOW_UPDATE "$aws/things/Hits_Scanner/shadow/update"
#define AWS_IOT_SHADOW_UPDATE_DELTA "$aws/things/Hits_Scanner/shadow/update/delta"
#define AWS_IOT_SHADOW_UPDATE_ACCEPTED "$aws/things/Hits_Scanner/shadow/update/accepted"
#define AWS_IOT_SHADOW_UPDATE_REJECTED "$aws/things/Hits_Scanner/shadow/update/rejected"

// RFID Config
#define RST_PIN         LED_BUILTIN          // Configurable, see typical pin layout above
#define SS_PIN          5         // Configurable, see typical pin layout above

Buzzer buzzer(4);
Screen screen;

WiFiClientSecure net = WiFiClientSecure();


MQTTClient client = MQTTClient(256);

// Init array that will store new NUID
byte nuidPICC[4];

unsigned long lastMillis = 0;


MFRC522 rfid(SS_PIN, RST_PIN);  // Create MFRC522 instance

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");
  screen.printMessage("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
}

void connectAWS() {
  Serial.print("Connecting to AWS IOT");
  screen.printMessage("Connecting to AWS...");

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

  Serial.println("AWS IoT Connected!");

  screen.printMessage("AWS IoT Connected!");
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
  buzzer.beep();
  screen.printMessage("Scanned: " + uid);
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
  SPI.begin();

  if (!screen.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  screen.printMessage("Initialising...");

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
	if (!rfid.PICC_IsNewCardPresent()) {
		return;
	}

	// Select one of the cards
	if (!rfid.PICC_ReadCardSerial()) {
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