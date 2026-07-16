ESP32 Dial-Up Modem

Blah blah blah, here are the main details lol======================================================

This project implements a functional hardware modem on the ESP32 using basic Arduino libraries (WiFi.h, FS.h, SPIFFS.h, Arduino.h). It recreates the Bell 103 standard (300 baud) with real FSK tones via a piezo buzzer, while also bridging data over WiFi sockets. The modem supports AT-style commands and can operate in both offline (tone-only) and online (WiFi TCP bridge) modes.

Features

Bell 103 standard frequencies (1270 Hz for binary 1, 1070 Hz for binary 0).

AT-style command interface (AT, ATH, P <port>, S <host>, W, NW, SW).

WiFi credential storage using SPIFFS.

Offline mode: local tone generation only.

Online mode: TCP bridge to remote servers.

Escape sequence (+++) to drop back to command mode.

Hardware Requirements

ESP32 development board (tested on ESP32-WROOM).

Piezo buzzer connected to GPIO pin 18.

Serial terminal for command input/output.

Setup

Connect the piezo buzzer to GPIO 18.

Flash the provided source code to your ESP32.

Open a serial terminal at 115200 baud.

On boot, the modem will attempt to auto-connect using saved WiFi credentials.

If no credentials exist, use the W command to configure SSID and password.

Usage

AT → Responds with OK.

P <port> → Sets the TCP port (default 80).

S <host> → Sets the host and attempts connection.

W → Interactive WiFi setup (enter SSID and password).

NW → Disable WiFi.

SW → Restart WiFi using saved credentials.

ATH → Disconnect from current connection.

+++ → Escape from data mode back to command mode.

Example Session

AT
OK
P 80
OK
S example.com
CONNECT 300 (DATA BRIDGE ACTIVE)
GET / HTTP/1.0
