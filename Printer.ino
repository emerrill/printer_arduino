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


#include <Arduino.h>
#include <Streaming.h>
#include <SoftwareSerial.h>
#include <WiFlySerial.h>
#include <Adafruit_Thermal.h>
#include <avr/pgmspace.h>



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

String buffer = String();


void setup() {
  pinMode(PRINTER_GND, OUTPUT); digitalWrite(PRINTER_GND, LOW); // To also work w/IoTP printer
  

  Serial.begin(9600);
  Serial.println("Start");
  
  printer.begin(75);
  printer.sleep();


  WiFly.listen();
  WiFly.begin();
  //Serial << F("Starting WiFly...") <<  WiFly.getLibraryVersion(bufRequest, REQUEST_BUFFER_SIZE) << endl;
  
  #if !STANDALONE
    Serial.println("Association.");
  #else
    //printer.println("Print2");
  #endif


  WiFly.SendCommandSimple("set comm remote 0", "");
  WiFly.setAuthMode( WIFLY_AUTH_WPA2_PSK);
  WiFly.setJoinMode( WIFLY_JOIN_AUTO );
  WiFly.setDHCPMode( WIFLY_DHCP_ON );
  if (!WiFly.setSSID(WIFI_SSID)) {
    #if !STANDALONE
      Serial.println("SSID Error");
    #endif
  }
  if (!WiFly.setPassphrase(WIFI_PASSPHRASE)) {
    #if !STANDALONE
      Serial.println("Passphrase Error");
    #endif
  }
  if (!WiFly.join()) {
    #if !STANDALONE
      Serial.println("Association failed.");
    #endif
    while (1) {
      // Hang on failure.
      // TODO
    }
  }

  #if !STANDALONE
    Serial.println("Connected to network.");
  #endif
  
  printOne();

}


void loop() {
  
  while(Serial.available()) {
    Serial.read(); 
  }
  
  while (!Serial.available()) {
    
  }
  while(Serial.available()) {
    Serial.read(); 
  }
  
  printOne();
}

void makeConnection(String extras) {
  WiFly.listen();
  WiFly.closeConnection();
  if (WiFly.openConnection( REMOTE_SERVER )) {
    Serial.println("connected");
    delay(20);
    WiFly << "GET /printer.php?key=" << REMOTE_PASSWORD << extras << endl;
    WiFly << "HOST: " << REMOTE_SERVER << endl << endl;
  } else {
    Serial.println("connection failed");
  }
  
  loadResponse();
}

void printOne() {
  if (!checkPaper(false)) {
    printer.sleep();
    return;
  }
  
  buffer = String();
  makeConnection("");
  processResponse();
  
  printer.sleep();
  
  if (!checkPaper(true)) {
    return;
  }
}

void printAll() {
  if (!checkPaper(false)) {
    printer.sleep();
    return;
  }
  
  boolean stat;
  do {
    makeConnection("");
    stat = processResponse();
    if (!checkPaper(true)) {
      printer.sleep();
      return;
    }
  } while (stat);
  printer.sleep();
}

boolean checkPaper(boolean last) {
  printer.listen();
  printer.wake();
  if (!printer.hasPaper()) {
    if (hasPaper == true) {
      sendPaper(false, last);
    }
    //printer.sleep();
    return false;
  }
  
  if (hasPaper == false) {
    sendPaper(true, false);
  }
  //printer.sleep();
  return true;
}

void sendPaper(boolean stat ,boolean last) {
  if (stat) {
    makeConnection("&paper=1");
  } else {
    if (last) {
      makeConnection("&paper=0&missedlast=1");
    } else {
      makeConnection("&paper=0");
    }
  }
  
}

void loadResponse() {
  unsigned long TimeOut = millis() + 40000;
  
  while (  TimeOut > millis() && WiFly.isConnectionOpen() ) {
    if (  WiFly.available() > 0 ) {
      buffer += (char)WiFly.read();
    }
  }
  
  WiFly.closeConnection();
}

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

void processBlock(String block) {
  printer.listen();
  //printer.wake();
  int pos = 0;
  while ((pos = block.indexOf("|", pos)) != -1) {
    processLine(block.substring(pos, block.indexOf("\n", pos)));
    pos++;
  }
  
  //printer.sleep();
  //WiFly.listen();
}

void processLine(String line) {
  line.trim();

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
          break;
        case '2':
          printer.setSize('M');
          break;
        case '3':
          printer.setSize('L');
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
  Serial.println(line.substring(formatEnd+1));
  printer.println(line.substring(formatEnd+1));
  //delay(250);
  
}


void setDefault() {
  printer.online();
  printer.justify('L');
  printer.inverseOff();
  printer.doubleHeightOff();
  printer.setLineHeight(32);
  printer.boldOff();
  printer.underlineOff();
  printer.strikeOff();
  printer.setBarcodeHeight(50);
  printer.upsideDownOff();
  //setSize('s');
}
