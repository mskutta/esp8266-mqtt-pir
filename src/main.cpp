#ifdef ESP8266 || ESP32
  #define ISR_PREFIX ICACHE_RAM_ATTR
#else
  #define ISR_PREFIX
#endif

#if !(defined(ESP_NAME))
  #define ESP_NAME "sensor" 
#endif

#include <Arduino.h>

#include <ESP8266WiFi.h> // WIFI support
#include <ArduinoOTA.h> // Updates over the air

// WiFi Manager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> 

// MQTT
#include <PubSubClient.h>
char topic[40] = {0};

/* WIFI */
char hostname[32] = {0};

/* MQTT */
WiFiClient wifiClient;
PubSubClient client(wifiClient);
const char* broker = "10.81.95.165";

/* Pin States */
#define PIN_COUNT 4
int pins[PIN_COUNT] = {D1, D2, D5, D6};
int currentState;
int lastState[PIN_COUNT] = {HIGH, HIGH, HIGH, HIGH};

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(F("Config Mode"));
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("MQTT Connecting...");
    if (client.connect(hostname)) {
      Serial.println("MQTT connected");
    } else {
      Serial.print(".");
      delay(1000);
    }
  }
}

void setup()
{
  Serial.begin(9600);

  /* LED */
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  delay(1000);

  /* WiFi */
  sprintf(hostname, "%s-%06X", ESP_NAME, ESP.getChipId());
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  if(!wifiManager.autoConnect(hostname)) {
    Serial.println("WiFi Connect Failed");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  } 

  Serial.println(hostname);
  Serial.print(F("  "));
  Serial.print(WiFi.localIP());
  Serial.print(F("  "));
  Serial.println(WiFi.macAddress());

  /* OTA */
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) { Serial.println(F("Auth Failed")); }
    else if (error == OTA_BEGIN_ERROR) { Serial.println(F("Begin Failed")); }
    else if (error == OTA_CONNECT_ERROR) { Serial.println(F("Connect Failed")); }
    else if (error == OTA_RECEIVE_ERROR) { Serial.println(F("Receive Failed")); } 
    else if (error == OTA_END_ERROR) { Serial.println(F("End Failed")); }
  });
  ArduinoOTA.begin();

  // pins
  pinMode(D1, INPUT_PULLUP); 
  pinMode(D2, INPUT_PULLUP); 
  // pinMode(D3, INPUT_PULLUP); // Pulled high for boot. Unable to use with PIR.
  // pinMode(D4, INPUT_PULLUP); // Pulled high for boot. Unable to use with PIR.
  pinMode(D5, INPUT_PULLUP); 
  pinMode(D6, INPUT_PULLUP); 
  // pinMode(D7, INPUT_PULLUP); 
  // pinMode(D8, INPUT_PULLUP); // Pulled low for boot. Unable to use with PIR.
  
  /* MQTT */
  client.setServer(broker, 1883);

  /* LED */
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop()
{
  ArduinoOTA.handle();
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  // loop through pins
  for(int i = 0; i < PIN_COUNT; i++) {
    currentState = digitalRead(pins[i]);
    if (currentState != lastState[i]) {
      lastState[i] = currentState;
      sprintf(topic, "%s/%d", ESP_NAME, i);
      Serial.print(topic);
      if (currentState == HIGH) {
        client.publish(topic, "1");
        Serial.println(F(" HIGH"));
      } else {
        client.publish(topic, "0");
        Serial.println(F(" LOW"));
      }
    }
  }
}