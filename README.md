# IoTCRHTTPS

IoT local HTTPS development using the Computado Rita<br>
[HowTo](https://shaunsavage.substack.com/p/using-the-computado-rita-as-a-esp32)

## Goal

To have the IoT device GET and POST a URL to a HTTPS web server over WiFi.

## Need to do;

Create a PKI certificate authority and server certificate<br>
Create a WiFi Access Point (AP) (Hotspot)<br>
Install and configure nginx web server and time server<br>
Install [esp-idf](https://github.com/espressif/esp-idf), a ESP32 deveopment system<br>
[Clone this repository](https://github.com/greenpdx/IoTCRHTTPS.git)<br>
Setup ESP32 to connect to Wifi<br>
Setup ESP32 to connect to https server<br>

## Equipment

Computado Rita as server ; RAM 4G or 8G<br>
Seeed Xiao ESP32C3 or just change the configurations other ESP32,<br>
USB-A to USB-C cable<br>

There are many HowTos on installing and using Arduino IDE, but the Arduino IDE doesn’t give the developer the power needed to make a advance IoT applications.  This HowTo uses esp-idf to develop a Wifi connected application to the Computado Rita. The Seeed Xiao ESP32C31 is good choice of processor because of the many features it uses and is Arduino compatiable.  The ESP32C32 has Wifi, BlueTooth, I2C, SPI and serial.

Started with the https_request example from the esp-idf repository.  ( examples/protocols/https_request). Because this is a closed IoT network, I wanted to isolate it from the internet for security. This means the Computado Rita needs to handle all network services for the IoT network.

## Setup nginx to echo POST requests
A POST request is a way for a client to send information to the web server. Usually there is a special web server program to accept POST data. Nginx has a echo module that we can use for testing a POST request.<br>
First install the mod-http-echo module<br>
``` $ sudo apt install nginx-extra```<br>
This command install many different modules including the echo.<br>
Edit /etc/nginx/sites-enabled/default and add a POST echo location<br>
```
 location /echo {
    echo_read_request_body;
    echo_request_body;
    default_type application/json;
    chunked_transfer_encoding off;
 }
```
Restart nginx<br>
``` $ sudo systemctl restart nginx```
