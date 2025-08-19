# IoTCRHTTPS

IoT local HTTPS development using the Computado Rita

## Need to do;

Create a PKI certificate authority
How to create a WiFi Access Poing (Hotspot)
Install and configure to create a https web server and time server
Install esp-idf a ESP32 deveopment system
Setup ESP32 to connect to Wifi
Setup ESP32 to connect to https server

## Equipment

Computado Rita as server ; RAM 4G or 8G
Seeed Xiao ESP32C3 or just change the configurations other ESP32,
USB-A to USB-C cable

## The steps are, after CR installed

Setup Access Point (hotspot) on the Computado Rita, force to use WPA2
Install nginx web server, time server
Create and install root CA and crypto for SSL web server
Install esp-idf
Clone IoTCRHTTPS, https://github.com/greenpdx/IoTCRHTTPS
Copy root CA to project
Build, flash and run (monitor)  IoTCRHTTPS

## Goal

To have the IoT device read a URL from the CR web server over WiFi.

There are many HowTos on installing and using Arduino IDE, but the Arduino IDE doesn’t give the developer the power needed to make a advance IoT applications.  This HowTo uses esp-idf to develop a Wifi connected application to the Computado Rita. The Seeed Xiao ESP32C31 is good choice of processor because of the many features it uses and is Arduino compatiable.  The ESP32C32 has Wifi, BlueTooth, I2C, SPI and serial.

Started with the https_request example from the esp-idf repository.  ( examples/protocols/https_request). Because this is a closed IoT network, I wanted to isolate it from the internet for security. This means the RPi needs to handle all network services for the IoT network.
