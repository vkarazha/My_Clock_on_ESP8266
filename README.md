# My_Clock_on_ESP8266

This is the source and schema for homemade digital clock I've made.

![alt text](https://github.com/vkarazha/My_Clock_on_ESP8266/blob/master/Schema.png)

![alt text](https://github.com/vkarazha/My_Clock_on_ESP8266/blob/master/LEDs.png)

| Components                                 |
| -------------                          	   |
| Wemos D1 mini                              |
| LED strip WS2812B 1 meter 60 RGB LED's		   |
| Temperature Sensor: DS18b20               	|
| 5V / 2A  Power Supply								              |
| Wires, Glue and a lot of patience :)       |
 
### Important

Create a file in the same folder as the .ino file with the name "Settings.h" and add and change the following code to be able to connect to your Wi-Fi network:

```
#define SID "ssid"
#define PSW "password"
``` 
