/*
  MQTT_Communication.cpp - A simple client for MQTT.
*/

#include "MQTT_Communication.h"
#include "Arduino.h"

//MQTT_Communication constructor
// Initializes the MQTT_Communication object, setting the initial state to "MQTT_DISCONNECTED", the client and buffer to NULL, and other settings
MQTT_Communication::MQTT_Communication() {
    this->_state = MQTT_DISCONNECTED;
    this->_client = NULL;
    this->stream = NULL;
    setCallback(NULL);
    this->bufferSize = 0;
    setBufferSize(MQTT_MAX_PACKET_SIZE);
    setKeepAlive(MQTT_KEEPALIVE);
    setSocketTimeout(MQTT_SOCKET_TIMEOUT);
}

// MQTT_Communication constructor with a client
// Initializes the MQTT_Communication object, setting the initial state to "MQTT_DISCONNECTED" and specifying the client
MQTT_Communication::MQTT_Communication(Client& client) {
    this->_state = MQTT_DISCONNECTED;
    setClient(client);
    this->stream = NULL;
    this->bufferSize = 0;
    setBufferSize(MQTT_MAX_PACKET_SIZE);
    setKeepAlive(MQTT_KEEPALIVE);
    setSocketTimeout(MQTT_SOCKET_TIMEOUT);
}

// MQTT_Communication destructor
// Frees dynamically allocated memory for the buffer
MQTT_Communication::~MQTT_Communication() {
  free(this->buffer);
}

///////////////////////////////////////////////////////////
// Function: connect
// Description: Connects the client.
///////////////////////////////////////////////////////////
// Input:
// - clientID - the client ID to use when connecting to the server
//
// Credentials - (optional)
// - username - the username to use. If NULL, no username or password is used
// - password - the password to use. If NULL, no password is used
//
// Will - (optional)
// - willTopic - the topic to be used by the will message
// - willQoS: 0,1 or 2 - the quality of service to be used by the will message
// - willRetain - whether the will should be published with the retain flag
// - willMessage - the payload of the will message
// - cleanSession (optional) - whether to connect clean-session or not
//
// Output:
//   Returns true if the connection succeeded; otherwise, false.
//
boolean MQTT_Communication::connect(const char *id) {
    return connect(id,NULL,NULL,0,0,0,0,1);
}

boolean MQTT_Communication::connect(const char *id, const char *user, const char *pass) {
    return connect(id,user,pass,0,0,0,0,1);
}

boolean MQTT_Communication::connect(const char *id, const char *user, const char *pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage, boolean cleanSession) {
    if (!connected()) {
        int result = 0;

        if(_client->connected()) {
            result = 1;
        // Depending on whether a domain is defined or not, the function tries to connect to the server using the domain or IP    
        } else {
            if (domain != NULL) {
                result = _client->connect(this->domain, this->port);
            } else {
                result = _client->connect(this->ip, this->port);
            }
        }

        if (result == 1) {
            nextMsgId = 1;
            uint16_t length = MQTT_MAX_HEADER_SIZE;
            unsigned int j;


//Entering the right version into the header
#if MQTT_VERSION == MQTT_VERSION_3_1_1
            uint8_t d[7] = {0x00,0x04,'M','Q','T','T',MQTT_VERSION};
#define MQTT_HEADER_VERSION_LENGTH 7
#endif
            
            //Initialize the CONNECT message header in the buffer
            for (j = 0;j<MQTT_HEADER_VERSION_LENGTH;j++) {
                this->buffer[length++] = d[j];
            }

            uint8_t v;
            if (willTopic) {
                v = 0x04|(willQos<<3)|(willRetain<<5);
            } else {
                v = 0x00;
            }
            if (cleanSession) {
                v = v|0x02;
            }

            if(user != NULL) {
                v = v|0x80;

                if(pass != NULL) {
                    v = v|(0x80>>1);
                }
            }

            this->buffer[length++] = v;
            this->buffer[length++] = ((this->keepAlive) >> 8);
            this->buffer[length++] = ((this->keepAlive) & 0xFF);

            //Adding the customer ID, topic and the content
            CHECK_STRING_LENGTH(length,id)
            length = writeString(id,this->buffer,length);
            if (willTopic) {
                CHECK_STRING_LENGTH(length,willTopic)
                length = writeString(willTopic,this->buffer,length);
                CHECK_STRING_LENGTH(length,willMessage)
                length = writeString(willMessage,this->buffer,length);
            }

            if(user != NULL) {
                CHECK_STRING_LENGTH(length,user)
                length = writeString(user,this->buffer,length);
                if(pass != NULL) {
                    CHECK_STRING_LENGTH(length,pass)
                    length = writeString(pass,this->buffer,length);
                }
            }

            write(MQTTCONNECT,this->buffer,length-MQTT_MAX_HEADER_SIZE);

            lastInActivity = lastOutActivity = millis();

            //Waiting for confirmation from the server and considering three connection states
            while (!_client->available()) {
                unsigned long t = millis();
                if (t-lastInActivity >= ((int32_t) this->socketTimeout*1000UL)) {
                    _state = MQTT_CONNECTION_TIMEOUT;
                    _client->stop();
                    return false;
                }
            }
            uint8_t llen;
            uint32_t len = readPacket(&llen);

            if (len == 4) {
                if (buffer[3] == 0) {
                    lastInActivity = millis();
                    pingOutstanding = false;
                    _state = MQTT_CONNECTED;
                    return true;
                } else {
                    _state = buffer[3];
                }
            }
            _client->stop();
        } else {
            _state = MQTT_CONNECT_FAILED;
        }
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////
// Function: readByte
// Description: Reads a byte from the MQTT client connection.
///////////////////////////////////////////////////////////
// Input:
//   - result: A pointer to the variable where the read byte will be stored.
// Output:
//   Returns true if a byte is successfully read within the specified socketTimeout; otherwise, false.
//
boolean MQTT_Communication::readByte(uint8_t * result) {
   uint32_t previousMillis = millis();
   while(!_client->available()) {
     yield();
     uint32_t currentMillis = millis();
     if(currentMillis - previousMillis >= ((int32_t) this->socketTimeout * 1000)){
       return false;
     }
   }
   *result = _client->read();
   return true;
}

// Reads a byte into result[*index] and increments index
boolean MQTT_Communication::readByte(uint8_t * result, uint16_t * index){
  uint16_t current_index = *index;
  uint8_t * write_address = &(result[current_index]);
  if(readByte(write_address)){
    *index = current_index + 1;
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////
// Function: readPacket
// Description: Reads an MQTT message packet from the client, analyzes its header, then reads the rest of the packet, handling specific cases for the "PUBLISH" type
///////////////////////////////////////////////////////////
// Input:
//   - lengthLength: A pointer to the variable where the length of the packet will be stored.
// Output:
//   Returns the length of the packet if successfully read; otherwise, returns 0.
//
uint32_t MQTT_Communication::readPacket(uint8_t* lengthLength) {
    uint16_t len = 0;
    if(!readByte(this->buffer, &len)) return 0;
    bool isPublish = (this->buffer[0]&0xF0) == MQTTPUBLISH;
    uint32_t multiplier = 1;
    uint32_t length = 0;
    uint8_t digit = 0;
    uint16_t skip = 0;
    uint32_t start = 0;

    do {
        if (len == 5) {
            // Invalid remaining length encoding - kill the connection
            _state = MQTT_DISCONNECTED;
            _client->stop();
            return 0;
        }
        if(!readByte(&digit)) return 0;
        this->buffer[len++] = digit;
        length += (digit & 127) * multiplier;
        multiplier <<=7; //multiplier *= 128
    } while ((digit & 128) != 0);
    *lengthLength = len-1;

    if (isPublish) {
        // Read in topic length to calculate bytes to skip over for Stream writing
        if(!readByte(this->buffer, &len)) return 0;
        if(!readByte(this->buffer, &len)) return 0;
        skip = (this->buffer[*lengthLength+1]<<8)+this->buffer[*lengthLength+2];
        start = 2;
        if (this->buffer[0]&MQTTQOS1) {
            // skip message id
            skip += 2;
        }
    }
    uint32_t idx = len;

    for (uint32_t i = start;i<length;i++) {
        if(!readByte(&digit)) return 0;
        if (this->stream) {
            if (isPublish && idx-*lengthLength-2>skip) {
                this->stream->write(digit);
            }
        }

        if (len < this->bufferSize) {
            this->buffer[len] = digit;
            len++;
        }
        idx++;
    }

    if (!this->stream && idx > this->bufferSize) {
        len = 0; // This will cause the packet to be ignored.
    }
    return len;
}

///////////////////////////////////////////////////////////
// Function: loop
// Description: This should be called regularly to allow the client to process incoming messages and maintain its connection to the server.
///////////////////////////////////////////////////////////
// Input:
//   - None.
// Output:
//   Returns true if the client is still connected; otherwise, false.
//
boolean MQTT_Communication::loop() {
    if (connected()) {
        unsigned long t = millis();
        if ((t - lastInActivity > this->keepAlive*1000UL) || (t - lastOutActivity > this->keepAlive*1000UL)) {
            if (pingOutstanding) {
                this->_state = MQTT_CONNECTION_TIMEOUT;
                _client->stop();
                return false;
            } else {
                this->buffer[0] = MQTTPINGREQ;
                this->buffer[1] = 0;
                _client->write(this->buffer,2);
                lastOutActivity = t;
                lastInActivity = t;
                pingOutstanding = true;
            }
        }
        if (_client->available()) {
            uint8_t llen;
            uint16_t len = readPacket(&llen);
            uint16_t msgId = 0;
            uint8_t *payload;
            if (len > 0) {
                lastInActivity = t;
                uint8_t type = this->buffer[0]&0xF0;
                if (type == MQTTPUBLISH) {
                    if (callback) {
                        uint16_t tl = (this->buffer[llen+1]<<8)+this->buffer[llen+2]; /* topic length in bytes */
                        memmove(this->buffer+llen+2,this->buffer+llen+3,tl); /* move topic inside buffer 1 byte to front */
                        this->buffer[llen+2+tl] = 0; /* end the topic as a 'C' string with \x00 */
                        char *topic = (char*) this->buffer+llen+2;
                        if ((this->buffer[0]&0x06) == MQTTQOS1) {
                            msgId = (this->buffer[llen+3+tl]<<8)+this->buffer[llen+3+tl+1];
                            payload = this->buffer+llen+3+tl+2;
                            callback(topic,payload,len-llen-3-tl-2);

                            this->buffer[0] = MQTTPUBACK;
                            this->buffer[1] = 2;
                            this->buffer[2] = (msgId >> 8);
                            this->buffer[3] = (msgId & 0xFF);
                            _client->write(this->buffer,4);
                            lastOutActivity = t;

                        } else {
                            payload = this->buffer+llen+3+tl;
                            callback(topic,payload,len-llen-3-tl);
                        }
                    }
                } else if (type == MQTTPINGREQ) {
                    this->buffer[0] = MQTTPINGRESP;
                    this->buffer[1] = 0;
                    _client->write(this->buffer,2);
                } else if (type == MQTTPINGRESP) {
                    pingOutstanding = false;
                }
            } else if (!connected()) {
                // readPacket has closed the connection
                return false;
            }
        }
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////
// Function: publish 
// Description: Publishes an MQTT message to the specified topic.
///////////////////////////////////////////////////////////
// Input:
//   - topic: The MQTT topic to publish to.
//   - payload: The payload of the message as a string.
// Output:
//   Returns true if the message is successfully published; otherwise, false.
//
boolean MQTT_Communication::publish(const char* topic, const char* payload) {
    return publish(topic,(const uint8_t*)payload, payload ? strnlen(payload, this->bufferSize) : 0,false);
}


boolean MQTT_Communication::publish(const char* topic, const uint8_t* payload, unsigned int plength, boolean retained) {
    if (connected()) {
        if (this->bufferSize < MQTT_MAX_HEADER_SIZE + 2+strnlen(topic, this->bufferSize) + plength) {
            // Too long
            return false;
        }
        // Leave room in the buffer for header and variable length field
        uint16_t length = MQTT_MAX_HEADER_SIZE;
        length = writeString(topic,this->buffer,length);

        // Add payload
        uint16_t i;
        for (i=0;i<plength;i++) {
            this->buffer[length++] = payload[i];
        }

        // Write the header
        uint8_t header = MQTTPUBLISH;
        if (retained) {
            header |= 1;
        }
        return write(header,this->buffer,length-MQTT_MAX_HEADER_SIZE);
    }
    return false;
}


///////////////////////////////////////////////////////////
// Function: write 
// Description: Writes data to the buffer.
///////////////////////////////////////////////////////////
// Input:
//   - data: The single byte to be written.
// Output:
//   Returns the number of bytes written.
//
size_t MQTT_Communication::write(uint8_t data) {
    lastOutActivity = millis();
    return _client->write(data);
}

size_t MQTT_Communication::write(const uint8_t *buffer, size_t size) {
    lastOutActivity = millis();
    return _client->write(buffer,size);
}

boolean MQTT_Communication::write(uint8_t header, uint8_t* buf, uint16_t length) {
    uint16_t rc;
    uint8_t hlen = buildHeader(header, buf, length);
#ifdef MQTT_MAX_TRANSFER_SIZE
    uint8_t* writeBuf = buf+(MQTT_MAX_HEADER_SIZE-hlen);
    uint16_t bytesRemaining = length+hlen;  //Match the length type
    uint8_t bytesToWrite;
    boolean result = true;
    while((bytesRemaining > 0) && result) {
        bytesToWrite = (bytesRemaining > MQTT_MAX_TRANSFER_SIZE)?MQTT_MAX_TRANSFER_SIZE:bytesRemaining;
        rc = _client->write(writeBuf,bytesToWrite);
        result = (rc == bytesToWrite);
        bytesRemaining -= rc;
        writeBuf += rc;
    }
    return result;
#else
    rc = _client->write(buf+(MQTT_MAX_HEADER_SIZE-hlen),length+hlen);
    lastOutActivity = millis();
    return (rc == hlen+length);
#endif
}

///////////////////////////////////////////////////////////
// Function: buildHeader
// Description: Builds the header for an MQTT packet.
///////////////////////////////////////////////////////////
// Input:
//   - header: The MQTT packet header type.
//   - buf: The buffer to store the constructed header.
//   - length: The length of the MQTT packet payload.
// Output:
//   Returns the size of the constructed header.
//
size_t MQTT_Communication::buildHeader(uint8_t header, uint8_t* buf, uint16_t length) {
    uint8_t lenBuf[4];
    uint8_t llen = 0;
    uint8_t digit;
    uint8_t pos = 0;
    uint16_t len = length;
    do {

        digit = len  & 127; //digit = len %128
        len >>= 7; //len = len / 128
        if (len > 0) {
            digit |= 0x80;
        }
        lenBuf[pos++] = digit;
        llen++;
    } while(len>0);

    buf[4-llen] = header;
    for (int i=0;i<llen;i++) {
        buf[MQTT_MAX_HEADER_SIZE-llen+i] = lenBuf[i];
    }
    return llen+1; // Full header size is variable length bit plus the 1-byte fixed header
}


///////////////////////////////////////////////////////////
// Function: subscribe
// Description: Subscribes to an MQTT topic with the specified quality of service (QoS).
///////////////////////////////////////////////////////////
// Input:
//   - topic: The MQTT topic to subscribe to.
// Output:
//   Returns true if the subscription is successful; otherwise, false.
//
boolean MQTT_Communication::subscribe(const char* topic) {
    return subscribe(topic,0);
}

boolean MQTT_Communication::subscribe(const char* topic, uint8_t qos) {
    size_t topicLength = strnlen(topic, this->bufferSize);
    if (topic == 0) {
        return false;
    }
    if (qos > 1) {
        return false;
    }
    if (this->bufferSize < 9 + topicLength) {
        // Too long
        return false;
    }
    if (connected()) {
        // Leave room in the buffer for header and variable length field
        uint16_t length = MQTT_MAX_HEADER_SIZE;
        nextMsgId++;
        if (nextMsgId == 0) {
            nextMsgId = 1;
        }
        this->buffer[length++] = (nextMsgId >> 8);
        this->buffer[length++] = (nextMsgId & 0xFF);
        length = writeString((char*)topic, this->buffer,length);
        this->buffer[length++] = qos;
        return write(MQTTSUBSCRIBE|MQTTQOS1,this->buffer,length-MQTT_MAX_HEADER_SIZE);
    }
    return false;
}

///////////////////////////////////////////////////////////
// Function: disconnect
// Description: Disconnects the MQTT client from the broker.
///////////////////////////////////////////////////////////
// Input:
//   None.
// Output:
//   None.
//
void MQTT_Communication::disconnect() {
    this->buffer[0] = MQTTDISCONNECT;
    this->buffer[1] = 0;
    _client->write(this->buffer,2);
    _state = MQTT_DISCONNECTED;
    _client->flush(); // is used to clear or flush the output buffer of a stream
    _client->stop();
    lastInActivity = lastOutActivity = millis();
}

///////////////////////////////////////////////////////////
// Function: writeString
// Description: Writes a string to the specified buffer, including its length.
///////////////////////////////////////////////////////////
// Input:
//   - string: The null-terminated string to be written.
//   - buf: The buffer where the string and its length will be written.
//   - pos: The current position in the buffer.
// Output:
//   Returns the updated position in the buffer after writing the string and its length.
//
uint16_t MQTT_Communication::writeString(const char* string, uint8_t* buf, uint16_t pos) {
    const char* idp = string;
    uint16_t i = 0;
    pos += 2;
    while (*idp) {
        buf[pos++] = *idp++;
        i++;
    }
    buf[pos-i-2] = (i >> 8);
    buf[pos-i-1] = (i & 0xFF);
    return pos;
}

///////////////////////////////////////////////////////////
// Function: connected
// Description: Checks if the MQTT client is connected to the broker.
///////////////////////////////////////////////////////////
// Input:
//   None.
// Output:
//   Returns true if the client is connected; otherwise, false.
//
boolean MQTT_Communication::connected() {
    boolean rc;
    if (_client == NULL ) {
        rc = false;
    } else {
        rc = (int)_client->connected();
        if (!rc) {
            if (this->_state == MQTT_CONNECTED) {
                this->_state = MQTT_CONNECTION_LOST;
                _client->flush();
                _client->stop();
            }
        } else {
            return this->_state == MQTT_CONNECTED;
        }
    }
    return rc;
}

///////////////////////////////////////////////////////////
// Function: setServer
// Description: Sets the MQTT server information.
///////////////////////////////////////////////////////////
// Input:
//   - ip/domain: An array of four bytes representing the IP address of the server (for IPv4)/const char defining the domain
//   - port: The port number to connect to the server, 1883 for MQTT data transmission
// Output:
//   Returns a reference to the MQTT_Communication object after setting the server information.
//
MQTT_Communication& MQTT_Communication::setServer(uint8_t * ip, uint16_t port) {
    IPAddress addr(ip[0],ip[1],ip[2],ip[3]);
    return setServer(addr,port);
}

MQTT_Communication& MQTT_Communication::setServer(IPAddress ip, uint16_t port) {
    this->ip = ip;
    this->port = port;
    this->domain = NULL;
    return *this;
}

MQTT_Communication& MQTT_Communication::setServer(const char * domain, uint16_t port) {
    this->domain = domain;
    this->port = port;
    return *this;
}

///////////////////////////////////////////////////////////
// Function: setCallback
// Description: Sets the callback function for handling MQTT messages.
///////////////////////////////////////////////////////////
// Input:
//   - callback: a pointer to a message callback function called when a message arrives for a subscription created by this client
// Output:
//   Returns a reference to the MQTT_Communication object after setting the callback function.
//
MQTT_Communication& MQTT_Communication::setCallback(MQTT_CALLBACK_SIGNATURE) {
    this->callback = callback;
    return *this;
}

///////////////////////////////////////////////////////////
// Function: setClient
// Description: Sets the network client instance to use.
///////////////////////////////////////////////////////////
// Input:
//   - client: the network client to use, for example WiFiClient
// Output:
//   Returns the client instance after setting the client options.
//
MQTT_Communication& MQTT_Communication::setClient(Client& client){
    this->_client = &client;
    return *this;
}

///////////////////////////////////////////////////////////
// Function: state
// Description: Returns the current state of the client. If a connection attempt fails, this can be used to get more information about the failure.
///////////////////////////////////////////////////////////
// Input:
//   - None.
// Output:
//   Returns the current state of the client.
//
int MQTT_Communication::state() {
    return this->_state;
}

///////////////////////////////////////////////////////////
// Function: setBufferSize
// Description: Sets the size, in bytes, of the internal send/receive buffer. This must be large enough to contain the full MQTT packet.
///////////////////////////////////////////////////////////
// Input:
//   - size:  the size, in bytes, for the internal buffer; by default, it is set to 256 bytes - as defined by the MQTT_MAX_MESSAGE_SIZE constant
// Output:
//   returns a boolean flag to indicate whether it was able to reallocate the memory to change the buffer size
//
boolean MQTT_Communication::setBufferSize(uint16_t size) {
    if (size == 0) {
        // Cannot set it back to 0
        return false;
    }
    if (this->bufferSize == 0) {
        this->buffer = (uint8_t*)malloc(size);
    } else {
        uint8_t* newBuffer = (uint8_t*)realloc(this->buffer, size);
        if (newBuffer != NULL) {
            this->buffer = newBuffer;
        } else {
            return false;
        }
    }
    this->bufferSize = size;
    return (this->buffer != NULL);
}

///////////////////////////////////////////////////////////
// Function: getBufferSize
// Description: Gets the current size of the internal buffer.
///////////////////////////////////////////////////////////
// Input:
//   - None.
// Output:
//   the size of the internal buffer; by default, it is set to 256 bytes
//
uint16_t MQTT_Communication::getBufferSize() {
    return this->bufferSize;
}
MQTT_Communication& MQTT_Communication::setKeepAlive(uint16_t keepAlive) {
    this->keepAlive = keepAlive;
    return *this;
}
MQTT_Communication& MQTT_Communication::setSocketTimeout(uint16_t timeout) {
    this->socketTimeout = timeout;
    return *this;
}
