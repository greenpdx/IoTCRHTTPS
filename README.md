# IoTCRHTTPS

IoT local HTTPS development using the Computado Rita

## Need to do;

Create a PKI certificate authority<br>
How to create a WiFi Access Poing (Hotspot)<br>
Install and configure to create a https web server and time server<br>
Install esp-idf a ESP32 deveopment system<br>
Setup ESP32 to connect to Wifi<br>
Setup ESP32 to connect to https server<br>

## Equipment

Computado Rita as server ; RAM 4G or 8G<br>
Seeed Xiao ESP32C3 or just change the configurations other ESP32,<br>
USB-A to USB-C cable<br>

## The steps are, after CR installed

Setup Access Point (hotspot) on the Computado Rita, force to use WPA2<br>
Install nginx web server, time server<br>
Create and install root CA and crypto for SSL web server<br>
Install esp-idf<br>
Clone IoTCRHTTPS, https://github.com/greenpdx/IoTCRHTTPS<br>
Copy root CA to project<br>
Build, flash and run (monitor)  IoTCRHTTPS<br>

## Goal

To have the IoT device read a URL from the CR web server over WiFi.

There are many HowTos on installing and using Arduino IDE, but the Arduino IDE doesn’t give the developer the power needed to make a advance IoT applications.  This HowTo uses esp-idf to develop a Wifi connected application to the Computado Rita. The Seeed Xiao ESP32C31 is good choice of processor because of the many features it uses and is Arduino compatiable.  The ESP32C32 has Wifi, BlueTooth, I2C, SPI and serial.

Started with the https_request example from the esp-idf repository.  ( examples/protocols/https_request). Because this is a closed IoT network, I wanted to isolate it from the internet for security. This means the Computado Rita needs to handle all network services for the IoT network.

## Setup nginx to echo POST requests
A POST request is a way for a client to send information to the web server. Usually there is a special web server program to accept POST data. Nginx has a echo module that we can use for testing a POST request. 
First install the mod-http-echo module
 \$ sudo apt install nginx-extras
This command install many different modules including the echo.
Edit /etc/nginx/sites-enabled/default and add a POST echo location
'''
> location /echo {
>>	echo_read_request_body;
>>	echo_request_body;
>>	default_type application/json;
>>	chunked_transfer_encoding off;
>}
'''
Restart nginx
 \$ sudo systemctl restart nginx