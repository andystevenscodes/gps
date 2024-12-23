#include <Wire.h>                          // I2C for GNSS
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> // GNSS library
#include <MKRWAN.h>                         // LoRa library for MKR WAN
#include "arduino_secrets.h"                // TTN credentials

// Global variables
float latitude;
float longitude;
float altitude;
int satellites;
LoRaModem modem;  // Create an instance of the LoRa modem

// GPS instance
SFE_UBLOX_GNSS myGNSS;

long lastTime = 0; // Simple timer to control GPS data retrieval frequency

void setup() {
  // Serial.begin(9600); // Uncomment for debugging
  // while (!Serial) { ; } // Wait for the serial port to connect (comment for battery power)

  // Initialize I2C
  Wire.begin();

  // Initialize GPS
  if (!myGNSS.begin()) {
    Serial.println("Failed to initialize GNSS! Check connections.");
    while (1);  // Stay in this loop if GPS is not working
  }

  // Set I2C output to UBX format to reduce NMEA output noise
  myGNSS.setI2COutput(COM_TYPE_UBX);
  myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT);

  // LoRa initialization
  if (!modem.begin(EU868)) { // Change to your desired region
    Serial.println("Failed to start LoRa module");
    while (1);  // Stay in this loop if LoRa fails to initialize
  }

  // Join the network using OTAA
  if (!modem.joinOTAA(appEui, appKey)) {
    Serial.println("Failed to join the network");
    while (1);  // Stay in this loop if LoRa join fails
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
  int32_t latInt = (int32_t)((latitude + 90.0) * 16777215 / 180.0);  // Scale to full 3-byte range
  int32_t lonInt = (int32_t)((longitude + 180.0) * 16777215 / 360.0); // Scale to full 3-byte range

  // Encode latitude and longitude into 3 bytes each
  txBuffer[0] = (latInt >> 16) & 0xFF;
  txBuffer[1] = (latInt >> 8) & 0xFF;
  txBuffer[2] = latInt & 0xFF;

  txBuffer[3] = (lonInt >> 16) & 0xFF;
  txBuffer[4] = (lonInt >> 8) & 0xFF;
  txBuffer[5] = lonInt & 0xFF;

  // Debug: Print encoded latitude and longitude
  Serial.print("Encoded Latitude: ");
  Serial.print(txBuffer[0], HEX); Serial.print(" ");
  Serial.print(txBuffer[1], HEX); Serial.print(" ");
  Serial.println(txBuffer[2], HEX);

  Serial.print("Encoded Longitude: ");
  Serial.print(txBuffer[3], HEX); Serial.print(" ");
  Serial.print(txBuffer[4], HEX); Serial.print(" ");
  Serial.println(txBuffer[5], HEX);

  // Encode altitude (2 bytes) - allowing for altitude values beyond 255 meters
  int16_t altInt = (int16_t)altitude;
  txBuffer[6] = (altInt >> 8) & 0xFF;
  txBuffer[7] = altInt & 0xFF;

  // Encode number of satellites
  txBuffer[8] = (uint8_t)satellites;

  // Debug: Print entire encoded payload
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
