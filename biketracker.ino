// include the GSM library
#include <HardwareSerial.h>

//char buffer[100]; // WHAT WE ARE READING INTO
 
 String buffer;
 
byte pos = 0;  //WHAT POSITION WE ARE AT IN THAT BUFFER

char sendernumber[20];

enum _state 
{
  IDLESTATE,
  SMS_RECEIVED,
  SMS_TEXT,
  GSM_LOCATION,
  GPS_LOCATION,
};

byte state = IDLESTATE;
boolean getGSM = false;
boolean getGPS = false;

//BASICALLY TO RESET THE BUFFER

void resetBuffer() 
{
  memset(buffer, 0, sizeof(buffer));
  pos = 0;
}

void setup() {
  //Begin serial comunication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);
  while(!Serial);
   
  //Begin serial communication with Arduino and SIM800
  Serial1.begin(9600);
  while(!Serial1);
  
  // Setup GSM
 
  Serial1.print("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n");
  delay(500);
  Serial1.print("AT+SAPBR=3,1,\"APN\",\"internet\"\r\n");
  delay(500);
  Serial1.print("AT+SAPBR=1,1\r\n");  
  delay(500);
  //Set SMS format to ASCII
  Serial1.write("AT+CMGF=1\r\n");
  delay(500);
  //Turn off echo
  Serial1.write("ATE0\r\n");
  delay(500);
  //Enable serial notification on sms arrival
  Serial1.print("AT+CNMI=1,2,0,0,0\r\n");
  
  delay(1000); // Make sure commands are processed
  
  flushSIM800();
  
  resetBuffer();
  
  Serial.println("Setup Complete!");
}

void loop() {
  
  if (Serial1.available()) {
    interpret(Serial1.read());
  }
  
}

void interpret (byte b) {
  buffer[pos++] = b;

  switch(state) {
    case IDLESTATE:
      // remove leading newlines
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
      else if (strcmp(buffer, "UNDER-VOLTAGE WARNNING") == 0 )
      { 
      //  sendSMS
      }
      break;
    case SMS_RECEIVED:
        if(b == '\n') {
        
        char *p = strtok(buffer, ",");
        strcpy(sendernumber, p);
       // Serial.print("sendernumber: ");
       // Serial.println(sendernumber);
        
        resetBuffer();
        
        state = SMS_TEXT;
        }
      break;
    case SMS_TEXT:
      if(b == '\n') {
        Serial.print("SMS-TEXT: ");
        Serial.println(buffer);
        
        if (strncmp(buffer, "GPS", 3) == 0) {
          getGPSLocation();
        }        
        else if (strncmp(buffer, "GSM", 3) == 0) {
          getGSMLocation();
        }
        else {
          sendSMS("Unknown command", sendernumber);  
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
        char *lon = strtok(NULL, ",");  //Second part is the longitude
        char *lat = strtok(NULL, ",");  //Third part is the latitude
        
        
        String str;
        
        str.concat("google.de/maps/place/");
        str.concat(lon[0);
        //strcat(str, test.substring(0,1));
     //   strcat(str, "°");
        
     //   strcat(str, lat);
     //   strcat(str, ",");
     //   strcat(str, lon);
        
        sendSMS(str, sendernumber);
        
        state = IDLESTATE;
        resetBuffer();
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

void flushSIM800() {
  while(Serial1.available())
       Serial1.read();
}

void getGPSLocation () {
  //enable GPS
  Serial1.print("AT+CGPSPWR=1\r\n");
  
  Serial1.print("AT+CGPSINF=0\r\n");
  //checkGPSFix = 1;
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
