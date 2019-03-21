/*
  Version 2 of the prototype
  Mostly based off of the example code provided.
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

  //Turn GPS on
  if (!fona.enableGPS(true)) {
    Serial.println(F("Failed to turn GPS on"));
  }

  //
}

int repeatBeforeReport = 5; //# of times it should loop before sending info to phone. In essence, it's how long before updating the phone.
int iteration = 0; //Which loop it is on.
char gpsConverted[120];

//Needed for the respond to sms
char fonaNotificationBuffer[64];          //for notifications from the FONA
char smsBuffer[250];

void loop() {
  char gpsdata[120];

  //Check for GPS location
  fona.getGPS(0, gpsdata, 120);

  //For debugging purposes, prints GPS to serial
  if (type == FONA808_V1)
    Serial.println(F("Reply in format: mode,longitude,latitude,altitude,utctime(yyyymmddHHMMSS),ttff,satellites,speed,course"));
  else
    Serial.println(F("Reply in format: mode,fixstatus,utctime(yyyymmddHHMMSS),latitude,longitude,altitude,speed,course,fixmode,reserved1,HDOP,PDOP,VDOP,reserved2,view_satellites,used_satellites,reserved3,C/N0max,HPA,VPA"));
  Serial.println("gpsdata");
  Serial.println(gpsdata);

  for (int i = 4; i < 44; i++) {
    gpsConverted[i - 4] = gpsdata[i]; //Only taking the utc time, longitude, and latitude from the gpsdata. I'm keeping the gpsdata variable incase we need it later.
  }
  gpsConverted[119] = 0; //Add a null value to the end of the char array. It needs a null character to end.
  Serial.println("gpsConverted");
  Serial.println(gpsConverted); //For debugging, print the selected data
  /*
    if (iteration = (repeatBeforeReport - 1)) {
    char sendto[21] = "8582436344"; //The phone number is my number. Please change it to yours before you test it on your own. I don't want to be spammed.
    //char message[141] = "test message";
    flushSerial();
    for (int i = 0; i < repeatBeforeReport; i++) {
      if (!fona.sendSMS(sendto, gpsConverted)) { //Sends out the signal. Replace gpsdata with message and uncomment the array above if you want to send your own message for debugging.
        Serial.println(F("Failed to send SMS"));
      } else {
        Serial.println(F("Sent!"));
      }
    }
    Serial.println(F("Resetting iteration."));
    iteration = 0;
    }
    else {
    iteration = iteration + 1;
    }
  */

  //Reply to an sms
  char* bufPtr = fonaNotificationBuffer;    //handy buffer pointer
  if (fona.available())      //any data available from the FONA?
  {
    int slot = 0;            //this will be the slot number of the SMS
    int charCount = 0;
    //Read the notification into fonaInBuffer
    do  {
      *bufPtr = fona.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer)-1)));
    
    //Add a terminal NULL to the notification string
    *bufPtr = 0;
    //Scan the notification string for an SMS received notification.
    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot)) {
      Serial.print("slot: "); Serial.println(slot);
      char callerIDbuffer[32];  //Store the SMS sender number here
      // Retrieve SMS sender address/phone number.
      if (! fona.getSMSSender(slot, callerIDbuffer, 31)) {
        Serial.println("Didn't find SMS message in slot!");
      }
      Serial.print(F("FROM: ")); Serial.println(callerIDbuffer);
        // Retrieve SMS value.
        uint16_t smslen;
        if (fona.readSMS(slot, smsBuffer, 250, &smslen)) { // pass in buffer and max len!
          Serial.println(smsBuffer);
        }
      //Send back an automatic response
      Serial.println("Sending reponse...");
      if (!fona.sendSMS(callerIDbuffer, gpsConverted)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("Sent!"));
      }
      //Delete the original msg after it is processed, otherwise we will fill up all the slots and then we won't be able to receive SMS anymore
      if (fona.deleteSMS(slot)) {
        Serial.println(F("OK!"));
      } else {
        Serial.print(F("Couldn't delete SMS in slot ")); Serial.println(slot);
        fona.print(F("AT+CMGD=?\r\n"));
      }
    }
  }

  //delay(ms/s * seconds/minute * #OfMinutes)
  delay(1000 * 10 * 1); //Delay

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
