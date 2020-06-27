#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <MFRC522.h>
#include <SPI.h>
#include <Wire.h>

#include "secrets.h"
#include "./Buzzer/Buzzer.h"
#include "./Screen/Screen.h"
#include "./Scanner/Scanner.h"

// The MQTT topics that this device should publish/subscribe
#define CARD_SCAN_PUB_TOPIC "hitsScanner/scan"
#define SCREEN_DISPLAY_SUB_TOPIC "hitsScanner/screenDisplay"

// Pin configuration
#define RFID_RST_PIN LED_BUILTIN
#define RFID_SS_PIN 5
#define BUZZER_DEFAULT_DATA_PIN 4

// Buzzer
Buzzer buzzer(BUZZER_DEFAULT_DATA_PIN);
// OLED screen
Screen screen;
// RFID reader
Scanner scanner(RFID_SS_PIN, RFID_RST_PIN);
// WiFi client
WiFiClientSecure net = WiFiClientSecure();
// MQTT client
MQTTClient client = MQTTClient(256);

// Init array that will store new NUID
byte nuidPICC[4];

unsigned long lastMillis = 0;

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

    // Subscribe to screen update topic
    client.subscribe(SCREEN_DISPLAY_SUB_TOPIC);

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

    client.publish(CARD_SCAN_PUB_TOPIC, jsonBuffer);
    buzzer.beep();
    screen.printMessage("Scanned: " + uid);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("incoming message...");
  Serial.println("incoming: " + topic + " - " + payload);

  screen.printMessage(payload);

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

    // Initialise SPI BUS
    SPI.begin();

    // Initialise OLED screen
    screen.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    screen.printMessage("Initialising...");

    // Initialise rfid reader
    scanner.PCD_Init();
    delay(4);
    scanner.PCD_DumpVersionToSerial();

    // Connect to the configured WiFi
    connectWiFi();

    // Configure WiFiClientSecure to use the AWS IoT device credentials
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);

    // Connect to the MQTT broker to AWS endpoint for Thing
    client.begin(AWS_IOT_ENDPOINT, 8883, net);

    // Create a message handler
    client.onMessage(messageHandler);

    // Connect device to AWS iot
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
    if (!scanner.PICC_IsNewCardPresent()) {
        return;
    }

    // Select one of the cards
    if (!scanner.PICC_ReadCardSerial()) {
        return;
    }

    /* Continue if successful card read... */

    //  Publish card read event
    publishScan(getUIDString(scanner.uid.uidByte, scanner.uid.size));

    // Halt PICC
    scanner.PICC_HaltA();

    // Stop encryption on PCD
    scanner.PCD_StopCrypto1();

    // Add a small delay to prevent scans in quick successions
    delay(1000);
}