#include <WiFi.h>
#include "MQTT_Communication.h"
#include <Wire.h>
#include <Adafruit_BMP085.h>

const char* ssid = "PanDa3.0";
const char* password = "siabada321";

const char *mqtt_broker = "192.168.137.157";
const char *topic = "esp32/led";
const char *mqtt_username = "admin";
const char *mqtt_password = "admin";
const int mqtt_port = 1883;

WiFiClient espClient;
MQTT_Communication client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

Adafruit_BMP085 bmp;

const int ledPin = 4;
const int sensorPin = 2;

void setup() {
  Serial.begin(115200);

  if (!bmp.begin()) {
	Serial.println("Could not find a valid BMP180 sensor, check wiring!");
	while (1) {}
  }
  setup_wifi();
  client.setServer(mqtt_broker, 1883);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp32-client";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the Mosquitto MQTT broker on RPI\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
        Serial.println("Mosquitto MQTT broker connected");
    } else {
        Serial.print("failed with state ");
        Serial.print(client.state());
        for (int i =0; i++; i < 5)
        {
          reconnect();
        }
        delay(2000);
    }
}

  pinMode(ledPin, OUTPUT);
  client.subscribe(topic);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print("Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  
  if (String(topic) == "esp32/led") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("esp32-client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/led");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
void loop() {
  client.loop();

 long now = millis();
  if (now - lastMsg > 500){
  lastMsg = now;
  Serial.print("Temperature = ");
  Serial.print(bmp.readTemperature());
  Serial.println(" *C");
  char tempString[8];
  dtostrf(bmp.readTemperature(), 1, 2, tempString);
  client.publish("esp32/temperature", tempString);
    
  Serial.print("Pressure = ");
  Serial.print(bmp.readPressure());
  Serial.println(" Pa");
  char pressureString[8];
  dtostrf(bmp.readPressure(), 1, 2, pressureString);
  client.publish("esp32/pressure", pressureString);
    
  Serial.print("Altitude = ");
  Serial.print(bmp.readAltitude());
  Serial.println(" meters");
  char altitudeString[8];
  dtostrf(bmp.readAltitude(), 1, 2, altitudeString);
  client.publish("esp32/altitude", altitudeString);

  Serial.print("Pressure at sealevel (calculated) = ");
  Serial.print(bmp.readSealevelPressure());
  Serial.println(" Pa");
  char sealevelString[8];
  dtostrf(bmp.readSealevelPressure(), 1, 2, sealevelString);
  client.publish("esp32/sealevel", sealevelString);

  Serial.print("Real altitude = ");
  Serial.print(bmp.readAltitude(103200)); // more precise measurement of altitude with the current sea level in  Cracow, 12:00, 21.01.2024
  Serial.println(" meters");
  char realalString[8];
  dtostrf(bmp.readAltitude(103200), 1, 2, realalString);
  client.publish("esp32/realal", realalString);
    
  Serial.println();
  }
}