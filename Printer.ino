/*************************************************************************
  This is an Arduino library for the Adafruit Thermal Printer.
  Pick one up at --> http://www.adafruit.com/products/597
  These printers use TTL serial to communicate, 2 pins are required.

  Adafruit invests time and resources providing this open source code.
  Please support Adafruit and open-source hardware by purchasing products
  from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution.
 *************************************************************************/

// If you're using Arduino 1.0 uncomment the next line:
//#include "SoftwareSerial.h"
// If you're using Arduino 23 or earlier, uncomment the next line:
//#include "NewSoftSerial.h"

#define STANDALONE 0

// Load Libraries
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <avr/pgmspace.h>

// Outside Libraries
#include <WiFlySerial.h>
#include <Adafruit_Thermal.h>
#include <Streaming.h>


// This should define: WIFI_SSID WIFI_PASSPHRASE REMOTE_SERVER REMOTE_PORT REMOTE_URI REMOTE_PASSWORD
#include "Credentials.h"

//Pins
//Green Wire
#define PRINTER_RX 10
//Yellow Wire
#define PRINTER_TX 9
//Black Wire
#define PRINTER_GND 8

#define WIFLY_RX 11
#define WIFLY_TX 12



//Misc
#define USE_DHCP 1
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };



Adafruit_Thermal printer(PRINTER_RX, PRINTER_TX);
WiFlySerial WiFly(WIFLY_RX, WIFLY_TX);

boolean hasPaper = true;
boolean printerAsleep = false;
boolean sizeChange = false;

String buffer = String();


void setup() {
  pinMode(PRINTER_GND, OUTPUT); digitalWrite(PRINTER_GND, LOW); // To also work w/IoTP printer
  

  Serial.begin(9600);
  Serial.println("Start");
  
  printer.begin(75);


  WiFly.listen();
  WiFly.begin();


  Serial.println(F("Starting WiFi association."));

  WiFly.SendCommandSimple("set comm remote 0", "");
  WiFly.setAuthMode( WIFLY_AUTH_WPA2_PSK);
  WiFly.setJoinMode( WIFLY_JOIN_AUTO );
  WiFly.setDHCPMode( WIFLY_DHCP_ON );
  if (!WiFly.setSSID(WIFI_SSID)) {
    Serial.println(F("SSID Error"));
  }
  if (!WiFly.setPassphrase(WIFI_PASSPHRASE)) {
    Serial.println(F("Passphrase Error"));
  }
  if (!WiFly.join()) {
    Serial.println(F("Association failed."));
    while (1) {
      // Hang on failure.
      // TODO
    }
  }

  Serial.println(F("Connected to network."));
  
  printAll();

}


// Loop
void loop() {

  int i = 0;

  if (hasPaper) {
    sleepPrinter();
    delay(60000);
  } else {
    // If we don't have paper, check every second.
    // Every 60, reconnect so the server gets a status.
    for (i = 0; i < 60; i++) {
      delay(1000);
      if (checkPaper(false)) {
        delay(10000);
        break;
      }
    }
    hasPaper = true;
  }

  printAll();
}

// Make a connection to the remote server
void makeConnection(String extras) {
  WiFly.listen();
  WiFly.closeConnection();
  #if defined (REMOTE_IP)
    if (WiFly.openConnection( REMOTE_IP )) {
  #else
    if (WiFly.openConnection( REMOTE_SERVER )) {
  #endif
  
    Serial.println(F("Connected"));
    delay(20);
    WiFly << F("GET /printer.php?key=") << REMOTE_PASSWORD << extras << F(" HTTP/1.1\r\n");
    WiFly << F("HOST: ") << REMOTE_SERVER << endl << endl;
  } else {
    Serial.println(F("Connection failed"));
  }
  
  loadResponse();
}

// Print all avalable messages
void printAll() {
  if (!checkPaper(false)) {
    return;
  }
  
  boolean stat;
  do {
    makeConnection("");
    stat = processResponse();
    if (!checkPaper(true)) {
      return;
    }
  } while (stat);
}

// Check the paper status
boolean checkPaper(boolean last) {
  wakePrinter();
  if (!printer.hasPaper()) {
    if (hasPaper == true) {
      Serial.println(F("No Paper."));
      sendPaper(false, last);
    }
    hasPaper = false;
    return false;
  }
  
  if (hasPaper == false) {
    Serial.println(F("Paper added."));
    sendPaper(true, false);
  }
  hasPaper = true;
  return true;
}

// Send the paper status to the server
void sendPaper(boolean stat ,boolean last) {
  if (stat) {
    makeConnection("&paper=1");
  } else {
    if (last) {
      makeConnection("&paper=0&last=1");
    } else {
      makeConnection("&paper=0");
    }
  }
  
  // Load and whipe the buffer.
  loadResponse();
  buffer = String();
}

// Load a response from the server
void loadResponse() {
  unsigned long TimeOut = millis() + 40000;
  char a, b, c = 0;
  boolean load = false;
  
  while (  TimeOut > millis() && WiFly.isConnectionOpen() ) {
    if (  WiFly.available() > 0 ) {
      if (load) {
        buffer += (char)WiFly.read();
      } else {
        c = b;
        b = a;
        a = (char)WiFly.read();
        if (a == '^' && b == 'S' && c == '^') {
          load = true;
          buffer += "^S^";
        }
      }
    }
  }
  Serial.println(buffer);
  WiFly.closeConnection();
}

// Process a response
boolean processResponse() {
  if (buffer.lastIndexOf("^E^") == -1) {
    return false;
  }
  String block = buffer.substring(buffer.indexOf("^S^")+4, buffer.lastIndexOf("^E^"));
  buffer = String();
  Serial.print(block);
  Serial.flush();
  processBlock(block);
  
  return true;
}

// Process/print a block of lines.
void processBlock(String block) {
  int pos = 0;
  while ((pos = block.indexOf("|", pos)) != -1) {
    processLine(block.substring(pos, block.indexOf("\n", pos)));
    pos++;
  }
}

// Process and print a formatted line.
void processLine(String line) {
  wakePrinter();
  
  line.trim();
  //Serial.println(line);
  int formatStart = line.indexOf("|");
  int formatEnd = line.indexOf(":");

  if ((formatEnd - formatStart) <= 1) {
    setDefault();
  } else {
    for (int i = formatStart+1; i < formatEnd; i++) {
      char c = line[i];
      switch (c) {
        case 'N':
          setDefault();
          break;
        case 'I':
          printer.inverseOn();
          break;
        case 'i':
          printer.inverseOff();
          break;
        case 'U':
          printer.upsideDownOn();
          break;
        case 'u':
          printer.upsideDownOff();
          break;
        case 'H':
          printer.doubleHeightOn();
          break;
        case 'h':
          printer.doubleHeightOff();
          break;
        case 'W':
          printer.doubleWidthOn();
          break;
        case 'w':
          printer.doubleWidthOff();
          break;
        case 'B':
          printer.boldOn();
          break;
        case 'b':
          printer.boldOff();
          break;
        case 'L':
          printer.underlineOn();
          break;
        case 'l':
          printer.underlineOff();
          break;
        case 'S':
          printer.strikeOn();
          break;
        case 's':
          printer.strikeOff();
          break;
        case '1':
          printer.setSize('S');
          sizeChange = false;
          break;
        case '2':
          printer.setSize('M');
          sizeChange = true;
          break;
        case '3':
          printer.setSize('L');
          sizeChange = true;
          break;
        case '7':
          printer.setLineHeight(28);
          break;
        case '8':
          printer.setLineHeight(32);
          break;
        case '9':
          printer.setLineHeight(36);
          break;
        case '0':
          printer.setLineHeight(40);
          break;
      }
    }
  }
  
  formatStart = line.indexOf(":");
  formatEnd = line.indexOf("$");
  
  if ((formatEnd - formatStart) <= 1) {
    printer.justify('L');
  } else {
    for (int i = formatStart+1; i < formatEnd; i++) {
      char c = line[i];
      switch (c) {
        case 'L':
          printer.justify('L');
          break;
        case 'R':
          printer.justify('R');
          break;
        case 'C':
          printer.justify('C');
          break;
      }
    }
  }
  printer.println(line.substring(formatEnd+1));
  
}

// Set the printer to the listenter, and wake the printer if it's alsleep.
void wakePrinter() {
  printer.listen();
  if (printerAsleep) {
    Serial.println(F("Wake printer"));
    Serial.flush();
    printer.wake();
    printerAsleep = false;
  }
}

// Set the printer to the listenter, and sleep the printer if it's awake.

void sleepPrinter() {
  printer.listen();
  if (!printerAsleep) {
    Serial.println(F("Sleep printer"));
    Serial.flush();
    printer.sleep();
    printerAsleep = true;
  }
}

// Set default printer styles
void setDefault() {
  wakePrinter();
  printer.online();
  printer.justify('L');
  printer.inverseOff();
  printer.doubleHeightOff();
  printer.doubleWidthOff();
  printer.setLineHeight(32);
  printer.boldOff();
  printer.underlineOff();
  printer.strikeOff();
  printer.setBarcodeHeight(50);
  printer.upsideDownOff();
  if (sizeChange) {
    printer.setSize('s');
    sizeChange = false;
  }
}
