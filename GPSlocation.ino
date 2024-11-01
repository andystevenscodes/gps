/*
  GNSS and LoRaWAN Integration with Arduino
  =========================================

  This project, developed by Andy Stevens from Ant-Networks, demonstrates how to integrate a GNSS (GPS) module with an Arduino MKR WAN series board to transmit real-time GPS data over LoRaWAN to The Things Network (TTN). This code utilizes Over-The-Air Activation (OTAA) to join the LoRa network and transmit GPS data (latitude, longitude, altitude, speed, and satellite count) in an optimized 9-byte format.

  Creator: Andy Stevens
  Organization: Ant-Networks
  Project Purpose: IoT Tracking Project
  Libraries Used:
    - Wire.h: for I2C communication with GNSS
    - SparkFun_u-blox_GNSS_Arduino_Library.h: to interface with GNSS
    - MKRWAN.h: for LoRaWAN communication with TTN
    - arduino_secrets.h: for secure storage of TTN credentials (appEui, appKey)

  Usage:
    - Connect GNSS and LoRa components to the Arduino MKR WAN board.
    - Load TTN credentials in `arduino_secrets.h`.
    - Upload this code to the board and open Serial Monitor for debugging.
    - Data will be transmitted to TTN every minute when a GPS signal is acquired.

  License: MIT License
*/


#include <Wire.h>                          // I2C for GNSS
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> // GNSS library
#include <MKRWAN.h>                         // LoRa library for MKR WAN
#include "arduino_secrets.h"                // TTN credentials

// Create an instance of the LoRa modem
LoRaModem modem;

// GPS instance
SFE_UBLOX_GNSS myGNSS;

// GPS data variables
float latitude;
float longitude;
float altitude;
float speed;
int satellites;

long lastTime = 0; // Simple timer to control GPS data retrieval frequency

void setup() {
  // Serial.begin(9600); // Comment this out when on battery power
  // while (!Serial) {
  //   ; // Wait for the serial port to connect -> comment it out while on the move
  // }

  // Initialize I2C
  Wire.begin();

  // Initialize GPS
  if (!myGNSS.begin()) {
    Serial.println("Failed to initialize GNSS! Check connections.");
    while (1);
  }

  // Set I2C output to UBX format to reduce NMEA output noise
  myGNSS.setI2COutput(COM_TYPE_UBX);
  myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT);

  // LoRa initialization
  if (!modem.begin(EU868)) { // Change to your desired region
    Serial.println("Failed to start LoRa module");
    while (1);
  }

  // Join the network using OTAA
  if (!modem.joinOTAA(appEui, appKey)) {
    Serial.println("Failed to join the network");
    while (1);
  }

  Serial.println("Successfully joined the network!");

  // Wait for a valid GPS signal
  waitForGPS();
}

void loop() {
  // Only check if GPS data is available once per second to reduce traffic
  if (millis() - lastTime > 1000) {
    lastTime = millis();
    
    // Read GPS values if available
    latitude = myGNSS.getLatitude() / 10000000.0;
    longitude = myGNSS.getLongitude() / 10000000.0;
    altitude = myGNSS.getAltitude() / 1000.0; // Convert to meters
    satellites = myGNSS.getSIV();

    // Print GPS values to Serial Monitor for debugging
    printGPSValues();

    // Send data to TTN
    sendData();
    delay(60000);  // Delay to avoid sending too frequently (1 minute)
  } else {
    Serial.println("Waiting for GPS signal...");
    delay(1000);  // Wait a second before checking again
  }
}

void waitForGPS() {
  Serial.println("Waiting for GPS signal...");
  
  while (true) {
    if (myGNSS.getSIV() >= 3) { // Wait for at least 3 satellites
      Serial.println("GPS signal acquired!");
      return;  // Exit the loop if we have a valid location
    }
    
    // Print status every second to indicate we're still waiting
    Serial.print(".");
    delay(1000);
  }
}

void printGPSValues() {
  Serial.print("Location: ");
  Serial.print(latitude, 7);
  Serial.print(", ");
  Serial.println(longitude, 7);
  Serial.print("Altitude: ");
  Serial.print(altitude);
  Serial.println(" m");
  Serial.print("Number of satellites: ");
  Serial.println(satellites);
  Serial.println();
}

void sendData() {
  // Prepare your 9-byte LoRa packet
  uint8_t txBuffer[9];

  // Scale latitude and longitude for compact transmission
  int32_t latInt = (int32_t)((latitude + 90.0) * 10000);  // Latitude scaled as 4 bytes
  int32_t lonInt = (int32_t)((longitude + 180.0) * 10000); // Longitude scaled as 4 bytes

  // Encode latitude and longitude
  txBuffer[0] = (latInt >> 16) & 0xFF;
  txBuffer[1] = (latInt >> 8) & 0xFF;
  txBuffer[2] = latInt & 0xFF;

  txBuffer[3] = (lonInt >> 16) & 0xFF;
  txBuffer[4] = (lonInt >> 8) & 0xFF;
  txBuffer[5] = lonInt & 0xFF;

  txBuffer[6] = (uint8_t)altitude;   // 1 byte for altitude
  txBuffer[7] = (uint8_t)speed;      // 1 byte for speed
  txBuffer[8] = (uint8_t)satellites; // 1 byte for satellites

  // Print payload data to Serial for debugging
  Serial.print("Payload data: ");
  for (int i = 0; i < 9; i++) {
    Serial.print(txBuffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Start the packet transmission
  modem.beginPacket();
  modem.write(txBuffer, sizeof(txBuffer));
  int err = modem.endPacket();

  if (err > 0) {
    Serial.println("Data sent successfully!");
  } else {
    Serial.print("Error sending data: ");
    Serial.println(err); // Print error code
  }
}
