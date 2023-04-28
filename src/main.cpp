
#include <Arduino.h>
#include <PubSubClient.h>
#include <Dps310.h>
#include <Wire.h>
#include "WiFiClientSecure.h"
#include "certificates.h"

const char* ssid        = "Nordvegr";        // your network SSID (name of wifi network)
const char* password    = "Redbull123";   // your network password
//const char* mqtt_server = "clusterunifiednamespace.sytes.net"; 
const char* mqtt_server = "unifiednamespace.sytes.net";  //Adress for your Mosquitto broker server, it must be the same adress that you set in Mosquitto.csr CN field
int port                = 8883;             //Port to your Mosquitto broker server. Dont forget to forward it in your router for remote access

char msg[256];

WiFiClientSecure client;
PubSubClient mqtt_client(client); 

#define SDA_PIN 14
#define SCL_PIN 13
Dps310 Dps310PressureSensor = Dps310();
unsigned long lastMsg = 0;
int16_t ret;
byte tempConfig = 49;

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect("ESP32 UNS IOTT")) {
      Serial.println("connected");
      // ... and resubscribe
      mqtt_client.subscribe("UNS/Hvl/IIoT/Commands/TemperatureUnits");
    } else {
      mqtt_client.print("failed, rc=");
      mqtt_client.print(mqtt_client.state());
      mqtt_client.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void initializeDPS310(){
  Wire.begin(static_cast<int>(SDA_PIN),static_cast<int>(SCL_PIN), static_cast<uint32_t>(4000)); //sda scl config, static cast dude to bug.
  Dps310PressureSensor.begin(Wire);
  //temperature measure rate (value from 0 to 7)
  //2^temp_mr temperature measurement results per second
  int16_t temp_mr = 2;
  //temperature oversampling rate (value from 0 to 7)
  //2^temp_osr internal temperature measurements per result
  //A higher value increases precision
  int16_t temp_osr = 2;
  //pressure measure rate (value from 0 to 7)
  //2^prs_mr pressure measurement results per second
  int16_t prs_mr = 2;
  //pressure oversampling rate (value from 0 to 7)
  //2^prs_osr internal pressure measurements per result
  //A higher value increases precision
  int16_t prs_osr = 2;
  //startMeasureBothCont enables background mode
  //temperature and pressure ar measured automatically
  //High precision and hgh measure rates at the same time are not available.
  //Consult Datasheet (or trial and error) for more information
  int16_t ret = Dps310PressureSensor.startMeasureBothCont(temp_mr, temp_osr, prs_mr, prs_osr);
}
void callback(char* topic, byte* payload, unsigned int length) {
  tempConfig = payload[0];
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print(payload[i]);
  }
  Serial.print("\ttempConfig set to: ");
  Serial.print(tempConfig);
  Serial.println();
}
float average (float * array, int len)  // assuming array is int.
{
  long sum = 0L ;  // sum will be larger than an item, long for safety.
  for (int i = 0 ; i < len ; i++)
    sum += array [i] ;
  return  ((float) sum) / len ;  // average will be fractional, so float may be appropriate.
}
void setup() {
  Serial.begin(115200);
  initializeDPS310();
  delay(100); 

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }

  Serial.print("Connected to ");
  Serial.println(ssid);

  //Set up the certificates and keys
  client.setCACert(CA_cert);          //Root CA certificate
  client.setCertificate(ESP_cert); //for client verification if the require_certificate is set to true in the mosquitto broker config
  client.setPrivateKey(ESP_key);  //for client verification if the require_certificate is set to true in the mosquitto broker config
  mqtt_client.setServer(mqtt_server, port);
  mqtt_client.setCallback(callback);
}

void loop() {
  uint8_t pressureCount = 20;
  float pressure[pressureCount];
  uint8_t temperatureCount = 20;
  float temperature[temperatureCount];
  unsigned long now = millis();

  if (!mqtt_client.connected()) {
  reconnect();
  }
  mqtt_client.loop();
  if (now - lastMsg > 2000) {
    lastMsg = now;

    int16_t ret = Dps310PressureSensor.getContResults(temperature, temperatureCount, pressure, pressureCount);
    if (ret == 0){
      if(tempConfig == 49){
        snprintf (msg, 256, "{ \"Timestamp\" : \"11:11:11\", \"Temperature\" : { \"value\" : %lg, \"units\" : \"degC\" }, \"Pressure\" : { \"value\" : %lg, \"units\" : \"Pascal\" } }", temperature[1],pressure[1]);
      }
      else if(tempConfig == 48){
        snprintf (msg, 256, "{ \"Timestamp\" : \"11:11:11\", \"Temperature\" : { \"value\" : %lg, \"units\" : \"F\" }, \"Pressure\" : { \"value\" : %lg, \"units\" : \"Pascal\" } }", ((temperature[1]*1.8)+32),pressure[1]);
      }
      Serial.println(msg);
      mqtt_client.publish("UNS/Hvl/IIoT/Status/Measurements", msg, true);
    }
  }
}