/*
 * Includes.h
 *
 *  Created on: 1 May 2019
 *      Author: Ralph
 */
#ifndef INCLUDES_H_
#define INCLUDES_H_

#include <Arduino.h>
#include <HardwareSerial.h>
#include <include/wl_definitions.h>
#include <IPAddress.h>
#include <pins_arduino.h>
#include <stdlib_noniso.h>
#include <sys/types.h>
#include <user_interface.h>
#include <WiFiClientSecureAxTLS.h>
#include <WString.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Using Adafruit library to drive Wemos Mini D1 64 x 48 OLED screen
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string>

// Email related includes
#include <base64.h>

// We're reading parameters from an SD card
#include <SD.h>

// My secret stuff (eg WiFi password)
#include "config.h"

// How we connect to your local wifi
#include <ESP8266WiFi.h>

// UDP library which is how we communicate with Time Server
#include <WiFiUdp.h>

// See Arduino Playground for details of this useful time synchronisation library
#include <TimeLib.h>
#include <Timezone.h>

#endif /* INCLUDES_H_ */
