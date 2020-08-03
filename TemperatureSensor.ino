#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "credentials.h"
#include <AsyncDelay.h>

#define ONE_WIRE_BUS D2
#define HOSTNAME "PoolTemperature"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWD;

char* overwatchTopic = "kolcun/outdoor/pool/temperature/overwatch";
char* stateTopic = "kolcun/outdoor/pool/temperature/state";
char onlineMessage[50] = "Temperature Sensor Online";
char* server = MQTT_SERVER;
char* mqttMessage;
int controllerId;
boolean ledEnabled = true;
char address[4];
char clientId[25] = "temperature-pool";

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AsyncDelay delay60s;

void setup() {
  Serial.begin(115200);

  setupOTA();
  sensors.begin();
  Serial.println(clientId);

  pubSubClient.setServer(server, 1883);

  if (!pubSubClient.connected()) {
    reconnect();
  }
  setupTemperature();
}

void setupTemperature() {
  pinMode(ONE_WIRE_BUS, INPUT);
  delay60s.start(60000, AsyncDelay::MILLIS);
  //first time seems to be slightly off
  sensors.requestTemperatures();
  readTemperature();
}

void reconnect() {
  while (!pubSubClient.connected()) {
    if (pubSubClient.connect(clientId, MQTT_USER, MQTT_PASSWD)) {
      Serial.println("Connected to MQTT broker");
      pubSubClient.publish(overwatchTopic, onlineMessage);
      //      Serial.print("sub to: '");
      //      Serial.print(controlTopic);
      //      Serial.println("'");
      //if (!pubSubClient.subscribe(controlTopic, 1)) {
      // Serial.println("MQTT: unable to subscribe");
      //}
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubSubClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

}

void readTemperature() {
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print(" Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  float currentTempC = sensors.getTempCByIndex(0);
  float currentTempF = sensors.getTempFByIndex(0);
  Serial.print("Temperature C is: ");
  Serial.println(currentTempC);
  Serial.print("Temperature C is: ");
  Serial.println(currentTempF);

  const int capacity = JSON_OBJECT_SIZE(3);
  StaticJsonDocument<capacity> doc;
  doc["temperature"]["C"] = currentTempC;
  doc["temperature"]["F"] = currentTempF;

  String output;
  serializeJson(doc, output);
  pubSubClient.publish(stateTopic, (uint8_t*) output.c_str(), output.length(), true);
}

void loop() {
  ArduinoOTA.handle();
  if (!pubSubClient.connected()) {
    reconnect();
  }

  if (delay60s.isExpired()) {
    readTemperature();
    delay60s.repeat();
  }

  pubSubClient.loop();
}


void setupOTA() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Wifi Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname(HOSTNAME);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
