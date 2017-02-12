#include <HardwareSerial.h>
#include <EEPROM.h>

char buffer[100]; // WHAT WE ARE READING INTO
byte pos = 0;  //WHAT POSITION WE ARE AT IN THAT BUFFER

char sendernumber[20];

byte eepromPos = 1;

enum _state 
{
  IDLESTATE,
  SMS_RECEIVED,
  SMS_TEXT,
  GSM_LOCATION,
  GPS_LOCATION,
  GPS_WAIT_FIX,
};

byte state = IDLESTATE;
boolean getGpsFix = false;

//For awaiting GPS Fix
unsigned long previousMillis = 0;
const long interval = 5000; //check every 5 seconds

//Reset the buffer
void resetBuffer() 
{
  memset(buffer, 0, sizeof(buffer));
  pos = 0;
}

void setup() {
  //Builtin LED off while initializing
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  //Begin serial comunication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);
  while(!Serial);
   
  //Begin serial communication with Arduino and SIM808
  Serial1.begin(9600);
  while(!Serial1);
  
  //Turn command echo off
  Serial1.write("ATE0\r\n");
  
  //Set SMS format to ASCII
  Serial1.write("AT+CMGF=1\r\n");

  //Enable serial notification on sms arrival
  Serial1.print("AT+CNMI=1,2,0,0,0\r\n");
  
  delay(1000); // Make sure commands are processed
  
  // LED on when initializing finished
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  
  if (Serial1.available()) {
    interpret(Serial1.read());
  }
  
  //If waiting for GPS fix, check every 5 seconds
  if(getGpsFix == true) {
    unsigned long currentMillis = millis();
    
    if (currentMillis - previousMillis >= interval) {
      // save the last time
      previousMillis = currentMillis;
      //Check GPS Status
      Serial1.print("AT+CGPSSTATUS?\r\n");
      Serial.println("Checking if GPS is available");
    }
  }
  
}

void interpret (byte b) {
  buffer[pos++] = b;

  switch(state) {
    case IDLESTATE:
      // remove leading newlines and throw away messages that cannot be interpreted
      if ( b == '\n' ) {
        resetBuffer();
      }
      // if SMS received
      if (strcmp(buffer, "+CMT: ") == 0 )
      {
        Serial.println("SMS received");
        
       // while(Serial1.read() != '\n'); 
        
        resetBuffer();
        
        state = SMS_RECEIVED;
      }
      else if (strcmp(buffer, "+CIPGSMLOC: ") == 0 )
      { 
        state = GSM_LOCATION; 
        resetBuffer();
      }
      else if (strcmp(buffer, "+CGPSINF: ") == 0 )
      { 
        state = GPS_LOCATION; 
        resetBuffer();
      }
      else if (strcmp(buffer, "+CGPSSTATUS: ") == 0 )
      { 
        state = GPS_WAIT_FIX;
        resetBuffer();
      }
      else if (strncmp(buffer, "UNDER-VOLTAGE", 13) == 0 )
      { 
        String number;
        int i=0;
        do {
          number += (char)EEPROM.read(i++);
        } while((char)EEPROM.read(i) != 'A');
        number += '"';
        sendSMS("Battery voltage low", number);
        bufferReset();
      }
      break;
    case SMS_RECEIVED:
        if(b == '\n') {
        
        char *p = strtok(buffer, ",");
        strcpy(sendernumber, p);
        
        resetBuffer();
        state = SMS_TEXT;
        }
      break;
    case SMS_TEXT:
      if(b == '\n') {
        Serial.print("SMS-TEXT: ");
        Serial.println(buffer);
        
        if (strncmp(buffer, "GPS", 3) == 0) {
          //enable GPS
          Serial1.print("AT+CGPSPWR=1\r\n");
          getGpsFix = true;
        }        
        else if (strncmp(buffer, "GSM", 3) == 0) {
          getGSMLocation();
        }
        else if (strncmp(buffer, "REGISTER", 8) == 0) {
          int i = 0;
          do {
            EEPROM.write(i, sendernumber[i]);
          } while(sendernumber[++i] != '"');
          EEPROM.write(i, 'A'); //termination
        }
        else {
          sendSMS("Unknown command. Valid commands are: GSM, GPS, REGISTER", sendernumber);  
        }
        
        state = IDLESTATE;
        resetBuffer();
      }
      break;
    case GSM_LOCATION:
      if(b == '\n') {        
        //Interpret return of AT-command
        char *p = strtok(buffer, ",");  //Discard the first part
        char *lon = strtok(NULL, ",");  //Second part is the longitude
        char *lat = strtok(NULL, ",");  //Third part is the latitude
        
        char str[160];
        strcpy(str, "google.de/maps/place/");
        strcat(str, lat);
        strcat(str, ",");
        strcat(str, lon);
        
        sendSMS(str, sendernumber);
        
        state = IDLESTATE;
        resetBuffer();
      }
      break;
      case GPS_LOCATION:
      if(b == '\n') {
        Serial.println(buffer);
      //Interpret return of AT-command
        char *p = strtok(buffer, ",");  //Discard the first part
        p = strtok(NULL, ",");          //Discard the second part
        char *lat = strtok(NULL, ",");  //Third part is the longitude
        p = strtok(NULL, ",");          //Discard the fourth part
        char *lon = strtok(NULL, ",");  //Fifth part is the latitude
        
        char str[160];
        strcpy(str, "google.de/maps/place/");
        strcat(str, lat);
        strcat(str, ",");
        strcat(str, lon);
        
        sendSMS(str, sendernumber);
        
        state = IDLESTATE;
        resetBuffer();
      }
      break;
    case GPS_WAIT_FIX:
      if(b == '\n') {
        if (strncmp(buffer, "Location 3D Fix", 15) == 0) {
          //Get GPS-Location
          Serial1.print("AT+CGPSINF=2\r\n");
          getGpsFix = false;
          resetBuffer();
          
          Serial.println("GPS available!!!");
        }
        state = IDLESTATE;
      }
    break;
  }
}

void sendSMS(String text, String number) {
  //Send new SMS command and message number
  String cmd = "AT+CMGS=" + number + "\r\n";
  Serial1.print(cmd);
  delay(1000);
   
  //Send SMS content
  Serial1.print(text);
  delay(1000);
   
  //Send Ctrl+Z / ESC to denote SMS message is complete
  Serial1.write((char)26);
  delay(1000);
}

void sendATCommand(String command) {
  //send command
  Serial1.print(command);
  //wait for the command to finish
  delay(1000);
  //flush buffer
  while(Serial1.available())
    Serial1.read();
}

void getGSMLocation () {
   // set bearer parameter
  Serial1.print("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n");
  delay(200);
  // set apn
  Serial1.print("AT+SAPBR=3,1,\"APN\",\"internet\"\r\n");
  delay(200);
  // activate bearer context
  Serial1.print("AT+SAPBR=1,1\r\n");
  delay(200);
  // triangulate
  Serial1.print("AT+CIPGSMLOC=1,1\r\n");
}
