<div align="center">
  <h1>MQTT client on ESP32 :space_invader:</h1>
  Network Programming final project
  <h3>Introduction :pushpin:</h3>
</div>
In this project, client-server communication has been implemented, using the application layer protocol - MQTT. The client can publish data, which is then sent to the MQTT broker. The broker forwards the data to the Node-RED application, enabling the visualization of readings from the pressure and temperature sensor. The client can also subscribe to data received from the broker. Upon receiving the relevant message, the client makes a decision to turn the LED on or off. Everything can operate quickly and independently thanks to the configuration of topics for subscribing or publishing data.
<div align="center">
  <h3>Components :jigsaw:</h3>
</div>

* ESP32 microcontroller
* Raspberry Pi
* BMP180 sensor
* LED diode
* Breadboard
* Resistors (220 Ω)

![PS_algorithm](https://github.com/grzeniux/MQTT_network_programming/assets/132613343/6e57261a-0d8a-41c0-b7a3-2cc143c265d3)

