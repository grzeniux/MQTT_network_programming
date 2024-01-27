/*
 MQTT_Communication.h - A simple client for MQTT.
*/

#ifndef MQTT_Communication_h
#define MQTT_Communication_h

#include <Arduino.h>
#include "IPAddress.h"  //ESP32 libraries
#include "Client.h"
#include "Stream.h"

#define MQTT_VERSION_3_1_1    4

// MQTT_VERSION : Pick the version
#ifndef MQTT_VERSION
#define MQTT_VERSION MQTT_VERSION_3_1_1
#endif

// MQTT_MAX_PACKET_SIZE : Maximum packet size. Override with setBufferSize().
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 256
#endif

// MQTT_KEEPALIVE : keepAlive interval in Seconds. Override with setKeepAlive()
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 15
#endif

// MQTT_SOCKET_TIMEOUT: socket timeout interval in Seconds. Override with setSocketTimeout()
#ifndef MQTT_SOCKET_TIMEOUT
#define MQTT_SOCKET_TIMEOUT 15
#endif

// Possible values for client.state()
#define MQTT_CONNECTION_TIMEOUT     -4
#define MQTT_CONNECTION_LOST        -3
#define MQTT_CONNECT_FAILED         -2
#define MQTT_DISCONNECTED           -1
#define MQTT_CONNECTED               0
#define MQTT_CONNECT_BAD_PROTOCOL    1
#define MQTT_CONNECT_BAD_CLIENT_ID   2
#define MQTT_CONNECT_UNAVAILABLE     3
#define MQTT_CONNECT_BAD_CREDENTIALS 4
#define MQTT_CONNECT_UNAUTHORIZED    5

#define MQTTCONNECT     1 << 4  // Client request to connect to Server

#define MQTTPUBLISH     3 << 4  // Publish message
#define MQTTPUBACK      4 << 4  // Publish Acknowledgment
#define MQTTSUBSCRIBE   8 << 4  // Client Subscribe request
#define MQTTUNSUBSCRIBE 10 << 4 // Client Unsubscribe request
#define MQTTPINGREQ     12 << 4 // PING Request
#define MQTTPINGRESP    13 << 4 // PING Response
#define MQTTDISCONNECT  14 << 4 // Client is Disconnecting

#define MQTTQOS0        (0 << 1)
#define MQTTQOS1        (1 << 1)
#define MQTTQOS2        (2 << 1)

// Maximum size of fixed header and variable length size header
#define MQTT_MAX_HEADER_SIZE 5

#if defined(ESP8266) || defined(ESP32)
#include <functional>
#define MQTT_CALLBACK_SIGNATURE std::function<void(char*, uint8_t*, unsigned int)> callback
#else
#define MQTT_CALLBACK_SIGNATURE void (*callback)(char*, uint8_t*, unsigned int)
#endif

#define CHECK_STRING_LENGTH(l,s) if (l+2+strnlen(s, this->bufferSize) > this->bufferSize) {_client->stop();return false;}

class MQTT_Communication : public Print {
private:
   Client* _client;
   uint8_t* buffer;
   uint16_t bufferSize;
   uint16_t keepAlive;
   uint16_t socketTimeout;
   uint16_t nextMsgId;
   unsigned long lastOutActivity;
   unsigned long lastInActivity;
   bool pingOutstanding;
   MQTT_CALLBACK_SIGNATURE;
   uint32_t readPacket(uint8_t*);
   boolean readByte(uint8_t * result);
   boolean readByte(uint8_t * result, uint16_t * index);
   boolean write(uint8_t header, uint8_t* buf, uint16_t length);
   uint16_t writeString(const char* string, uint8_t* buf, uint16_t pos);
   // Build up the header ready to send
   // Note: the header is built at the end of the first MQTT_MAX_HEADER_SIZE bytes, so will start
   //       (MQTT_MAX_HEADER_SIZE - <returned size>) bytes into the buffer
   size_t buildHeader(uint8_t header, uint8_t* buf, uint16_t length);
   IPAddress ip;
   const char* domain;
   uint16_t port;
   Stream* stream;
   int _state;
public:
   MQTT_Communication();
   MQTT_Communication(Client& client);
   ~MQTT_Communication();

   MQTT_Communication& setServer(IPAddress ip, uint16_t port);
   MQTT_Communication& setServer(uint8_t * ip, uint16_t port);
   MQTT_Communication& setServer(const char * domain, uint16_t port);

   MQTT_Communication& setCallback(MQTT_CALLBACK_SIGNATURE);
   MQTT_Communication& setClient(Client& client);

   MQTT_Communication& setKeepAlive(uint16_t keepAlive);
   MQTT_Communication& setSocketTimeout(uint16_t timeout);

   boolean setBufferSize(uint16_t size);
   uint16_t getBufferSize();

   boolean connect(const char* id);
   boolean connect(const char* id, const char* user, const char* pass);
   boolean connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage, boolean cleanSession);
   void disconnect();
   
   boolean publish(const char* topic, const char* payload);

   boolean publish(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained);

  
   virtual size_t write(uint8_t);
   virtual size_t write(const uint8_t *buffer, size_t size);

   boolean subscribe(const char* topic);
   boolean subscribe(const char* topic, uint8_t qos);
  
   boolean loop();
   boolean connected();
   int state();
};

#endif
