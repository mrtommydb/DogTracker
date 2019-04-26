/*
   Code for the Arduino Uno
   Remember to change the phone number in the code before you run. I will be upset if you spam my phone.
*/

#include "Adafruit_FONA.h"

#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4

// this is a large buffer for replies
char replybuffer[255];

#include <SoftwareSerial.h>
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
uint8_t type;

void setup() {
  while (!Serial);

  Serial.begin(115200);
  Serial.println(F("FONA basic test"));
  Serial.println(F("Initializing....(May take 3 seconds)"));

  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    while (1);
  }
  type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));
  switch (type) {
    case FONA800L:
      Serial.println(F("FONA 800L")); break;
    case FONA800H:
      Serial.println(F("FONA 800H")); break;
    case FONA808_V1:
      Serial.println(F("FONA 808 (v1)")); break;
    case FONA808_V2:
      Serial.println(F("FONA 808 (v2)")); break;
    case FONA3G_A:
      Serial.println(F("FONA 3G (American)")); break;
    case FONA3G_E:
      Serial.println(F("FONA 3G (European)")); break;
    default:
      Serial.println(F("???")); break;
  }

  // Print module IMEI number.
  char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("Module IMEI: "); Serial.println(imei);
  }

  fonaSerial->print("AT+CNMI=2,1\r\n"); //Set up the FONA to send a +CMTI notification when an SMS is received

  //Turn GPS on
  if (!fona.enableGPS(true)) {
    Serial.println(F("Failed to turn GPS on"));
  }
}

int repeatBeforeReport = 12 * 30; //# of times it should loop before sending info to phone. In essence, it's how long before updating the phone.
int iteration = 0; //Which loop it is on.

char gpsConverted1[120]; char gpsConverted2[120]; char gpsConverted3[120]; //Used for trimming the fat off of the raw data collected from the gps

void loop() {

  //Auto-Reply to an sms
  int8_t smsnum = fona.getNumSMS();
  if (smsnum < 0) { //If we get a -1 then something is wrong. Usually solved by letting the code run for a bit on it's own.
    Serial.println(F("Could not read # SMS"));
    return; //Returns to the start of the loop function. Basically refreshes until we can read how many sms's we have.
  } else {
    Serial.print(smsnum);
    Serial.println(F(" SMS's on SIM card!"));
  }
  if (smsnum != 0) { //If there is a sms waiting for us
    //Find GPS location
    char gpsdata[120];
    fona.getGPS(0, gpsdata, 120);
    //For debugging purposes, prints GPS to serial
    if (type == FONA808_V1)
      Serial.println(F("Reply in format: mode,longitude,latitude,altitude,utctime(yyyymmddHHMMSS),ttff,satellites,speed,course"));
    else
      Serial.println(F("Reply in format: mode,fixstatus,utctime(yyyymmddHHMMSS),latitude,longitude,altitude,speed,course,fixmode,reserved1,HDOP,PDOP,VDOP,reserved2,view_satellites,used_satellites,reserved3,C/N0max,HPA,VPA"));
    Serial.println("gpsdata");
    Serial.println(gpsdata);

    //Convert the gps data to our format. Only taking the utc time, longitude, and latitude from the gpsdata. I'm keeping the gpsdata variable incase we need it later.
    int gpsI = 4; int gpsConvI = 0;
    String converted;
    while ((gpsI < 44) && (gpsdata[gpsI] != ',')) {
      gpsConverted1[gpsConvI] = gpsdata[gpsI]; //upc time
      gpsI++; gpsConvI++;
    }
    converted += gpsConverted1; converted += ", coordinate ";
    gpsI++; gpsConvI = 0;
    while ((gpsI < 44) && (gpsdata[gpsI] != ',')) {
      gpsConverted2[gpsConvI] = gpsdata[gpsI]; //longitude
      gpsI++; gpsConvI++;
    }
    converted += gpsConverted2; converted += ", ";
    gpsI++; gpsConvI = 0;
    while ((gpsI < 44) && (gpsdata[gpsI] != ',')) {
      gpsConverted3[gpsConvI] = gpsdata[gpsI]; //lattitude
      gpsI++; gpsConvI++;
    }
    converted += gpsConverted3; converted += " end";
    gpsConverted1[119] = 0; gpsConverted2[119] = 0; gpsConverted3[119] = 0; //Add a null value to the end of the char array. It needs a null character to end.
    Serial.println("gpsConverted");
    Serial.println(converted); //For debugging, print the selected data
    char sendOut[120]; converted.toCharArray(sendOut, 120); //Convert to a char array so it can be sent via SMS

    uint8_t n = 1;
    while (n <= smsnum) {
      uint16_t smslen;
      char sender[25];
      Serial.print(F("\n\rReading SMS #")); Serial.println(n);
      uint8_t len = fona.readSMS(n, replybuffer, 250, &smslen);
      // if the length is zero, its a special case where the index number is higher
      // so increase the max we'll look at!
      if (len == 0) {
        Serial.println(F("[empty slot]"));
        n++;
        continue;
      }
      if (! fona.getSMSSender(n, sender, sizeof(sender))) {
        //If we failed to get the sender
        sender[0] = 0;
      }
      Serial.print(F("SMS #")); Serial.print(n);
      Serial.print(" ("); Serial.print(len); Serial.println(F(") bytes "));
      Serial.println(replybuffer);
      Serial.print(F("From: ")); Serial.println(sender);

      if (!fona.sendSMS(sender, sendOut)) { //Auto reply
        Serial.println(F("Failed to send SMS"));
      } else {
        Serial.println(F("Sent!"));
      }
      fona.deleteSMS(n); //Deletes the sms. There are limited slots for sms messages.
    }
  }

  char sendto[21] = "9099199191";
  if (iteration == repeatBeforeReport) {
    //Find GPS location
    char gpsdata[120];
    fona.getGPS(0, gpsdata, 120);
    //For debugging purposes, prints GPS to serial
    if (type == FONA808_V1)
      Serial.println(F("Reply in format: mode,longitude,latitude,altitude,utctime(yyyymmddHHMMSS),ttff,satellites,speed,course"));
    else
      Serial.println(F("Reply in format: mode,fixstatus,utctime(yyyymmddHHMMSS),latitude,longitude,altitude,speed,course,fixmode,reserved1,HDOP,PDOP,VDOP,reserved2,view_satellites,used_satellites,reserved3,C/N0max,HPA,VPA"));
    Serial.println("gpsdata");
    Serial.println(gpsdata);

    //Convert the gps data to our format. Only taking the utc time, longitude, and latitude from the gpsdata. I'm keeping the gpsdata variable incase we need it later.
    int gpsI = 4; int gpsConvI = 0;
    String converted;
    while ((gpsI < 44) && (gpsdata[gpsI] != ',')) {
      gpsConverted1[gpsConvI] = gpsdata[gpsI]; //upc time
      gpsI++; gpsConvI++;
    }
    converted += gpsConverted1; converted += ", coordinate ";
    gpsI++; gpsConvI = 0;
    while ((gpsI < 44) && (gpsdata[gpsI] != ',')) {
      gpsConverted2[gpsConvI] = gpsdata[gpsI]; //longitude
      gpsI++; gpsConvI++;
    }
    converted += gpsConverted2; converted += ", ";
    gpsI++; gpsConvI = 0;
    while ((gpsI < 44) && (gpsdata[gpsI] != ',')) {
      gpsConverted3[gpsConvI] = gpsdata[gpsI]; //lattitude
      gpsI++; gpsConvI++;
    }
    converted += gpsConverted3; converted += " end";
    gpsConverted1[119] = 0; gpsConverted2[119] = 0; gpsConverted3[119] = 0; //Add a null value to the end of the char array. It needs a null character to end.
    Serial.println("gpsConverted");
    Serial.println(converted); //For debugging, print the selected data
    char sendOut[120]; converted.toCharArray(sendOut, 120); //Convert to a char array so it can be sent via SMS
    if (!fona.sendSMS(sendto, sendOut)) { //Auto reply
      Serial.println(F("Failed to send SMS"));
    } else {
      Serial.println(F("Sent!"));
    }
    iteration = 0;
  } else {
    iteration ++;
  }

  //delay(ms/s * seconds/minute * #OfMinutes)
  delay(1000 * 5 * 1); //Delay set to 5 seconds for testing

  flushSerial();
  while (fona.available()) {
    Serial.write(fona.read());
  }
}


/*
  The following are predefined functions from the example code.
  I left them in because they could be useful.
*/

// flush input


void flushSerial() {
  while (Serial.available())
    Serial.read();
}
char readBlocking() {
  while (!Serial.available());
  return Serial.read();
}
uint16_t readnumber() {
  uint16_t x = 0;
  char c;
  while (! isdigit(c = readBlocking())) {
    //Serial.print(c);
  }
  Serial.print(c);
  x = c - '0';
  while (isdigit(c = readBlocking())) {
    Serial.print(c);
    x *= 10;
    x += c - '0';
  }
  return x;
}
uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout) {
  uint16_t buffidx = 0;
  boolean timeoutvalid = true;
  if (timeout == 0) timeoutvalid = false;

  while (true) {
    if (buffidx > maxbuff) {
      //Serial.println(F("SPACE"));
      break;
    }

    while (Serial.available()) {
      char c =  Serial.read();

      //Serial.print(c, HEX); Serial.print("#"); Serial.println(c);

      if (c == '\r') continue;
      if (c == 0xA) {
        if (buffidx == 0)   // the first 0x0A is ignored
          continue;

        timeout = 0;         // the second 0x0A is the end of the line
        timeoutvalid = true;
        break;
      }
      buff[buffidx] = c;
      buffidx++;
    }

    if (timeoutvalid && timeout == 0) {
      //Serial.println(F("TIMEOUT"));
      break;
    }
    delay(1);
  }
  buff[buffidx] = 0;  // null term
  return buffidx;
}
