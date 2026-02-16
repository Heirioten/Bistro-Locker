#include <WiFiS3.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <Servo.h>
#include "arduinoSecrets.h"

#define DEBUG 1

#define BAUD_RATE 9600
#define CONNECT_ATTEMPTS  5
#define CONNECT_RETRY_DELAY 5000

/***
 * Client request types
 */
#define MSG_NONE 0
#define MSG_REGISTER 1
#define MSG_DELIMITERS " ,\n"
#define MSG_BUFFLEN 1026

/***
 *  Token count for client request
 */
#define NUM_TOKENS 8

#define NONE 0
#define SCANNER 1
#define VALID 2
#define NOT_VALID 3

#define VALIDATION_TIME 10000

#define PN532_SCK  (2)
#define PN532_MOSI (3)
#define PN532_SS   (4)
#define PN532_MISO (5)

#define BUZZER_PIN 7
#define SERVO_PIN 8
#define BUTTON_PIN 10

enum State
{
  GET_REQUEST,
  PARSE_REQUEST,
  SENSE_RFID,
  SEND_RESPONSE,
  VALIDATE,
  UNLOCK,
  SENSE_CLOSE,
  LOCK
};

/*****
 *  FSM state variable
 **/
enum State state = GET_REQUEST;

WiFiServer server(80);            //server socket
WiFiClient connectionSocket = server.available();

// WiFiServer validationServer(200);
// WiFiClient validationSocket = validationSocket.available();

int status = WL_IDLE_STATUS;     // the Wifi radio's status
int attempts= 0;

char ssid[] = SECRET_SSID;     // the name of your network
char passwd[] = SECRET_PASS;

byte mac[6];                     // the MAC address of your Wifi shield

IPAddress ip;

/***
*  Holds message line from client
*  holds tokens parsed from message line
*/
String msgFromClient= "";
char *tokens[NUM_TOKENS];

String validationMsg = "";

/***
*  Time Management Information
*/
unsigned long beginOfTime= 0;
unsigned long currentTime= 0;
unsigned long oldTime= 0;

bool clientRequest= false;

/***
 *  Registration information
 *
 *  Contact IP addresses
 *  Contact port numbers
 *  Contact event
 *  Contact start
 *  Contact delta
 *  Contact duration
 */
IPAddress contactIP;
int contactPort;
int contactEvent;
unsigned long contactStart;
unsigned long contactDelta;
unsigned long contactDuration;

unsigned long eventStartTime= 0;
unsigned long eventStopTime= 0;
unsigned long lastFireTime= 0;

char messageBuffer[MSG_BUFFLEN];

String valStr = "";
WiFiClient notifier;     //for reaching out with notifications

int elapsedValidationTime = 0;

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

Servo door;

/******
Begin NFC RFID scanner, connect to the network, 
create server, and set pin modes for the buzzer and button
*/
void setup()
{
  Serial.begin(BAUD_RATE);

  while(!Serial)
  {
    ;
  }

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  
  if (!versiondata) 
  {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);

  /***
   * try to connect to network identified by SSID using
   * specified password
   */
  while( (status != WL_CONNECTED) && (attempts < CONNECT_ATTEMPTS) ) 
  {
    Serial.print("Attempting to connect on WPA2 personal net, SSID: ");
    Serial.println(ssid);

    /*********
     * Join the WiFi network
     **/
    status= WiFi.begin(ssid,passwd);
    attempts++;
    delay(CONNECT_RETRY_DELAY);

    if (status == WL_CONNECTED) 
    {
      Serial.println("Connected");
    } 
    else 
    {
      Serial.println("Couldn't get a wifi connection");
    }
  }

  server.begin();

  /******
   * Signal attached to WiFi network by printing status info
   **/
  printWifiStatus();

  pinMode(BUTTON_PIN, INPUT);
  door.attach(SERVO_PIN);

 /***
  *  mark the beginning of time
  */
  beginOfTime= millis();
}

/*
Finite State Machine
*/
void loop()
{
  printStateVars();

  switch(state)
  {
    case GET_REQUEST:
      state = GetRequest();
      break;
    case PARSE_REQUEST:
      state = ParseRequest();
      break;
    case SENSE_RFID:
      state = SenseRFID();
      break;
    case SEND_RESPONSE:
      state = SendResponse();
      break;
    case VALIDATE:
      state = Validate();
      break;
    case UNLOCK:
      state = Unlock();
      break;
    case SENSE_CLOSE:
      state = SenseClose();
      break;
    case LOCK:
      state = Lock();
      break;
  }
}

/*
Waiting to receive registration request from the python server
*/
State GetRequest()
{
  if(DEBUG)
  {
    Serial.println();
    Serial.println("GET_REQUEST");
    Serial.print("The IP Address is:  ");
    Serial.println(ip);
  }

  /***
  *  Accept registration message from client
  *  If the registration is received then
  *  transition to parsing the request,
  *  otherwise go back to sensor query.
  *
  *  1.  Check if connectionSocket is valid.
  *  2.  Check if client on other end of 
  *      connection socket has sent data.
  *      This is done by checking if receive buffer
  *      has data (i.e. nonzero byte count).  
  *      Don't wait if the client has nothing to say
  *      at the moment.  The buffer will hang around
  *      and eventually fill until connectionSocket 
  *      is torn down or closed.
  *
  *  Note:  the registration must be parsed
  *         before sending response to client
  */
  connectionSocket = server.available();

  if (connectionSocket && (!clientRequest) ) 
  {
    if (connectionSocket.available()) 
    {
      msgFromClient = fetchLine(connectionSocket);  //retrieve message from client  
      clientRequest= true;
      return PARSE_REQUEST;
    }

    clientRequest = false;
  } 

  return GET_REQUEST;
}

/*
Parse registration request and ensure it is registering the correct way
*/
State ParseRequest()
{
  if (DEBUG) 
  {
    Serial.println();
    Serial.println("PARSE_REQUEST");
  }

  /***
  *  First check the incoming connection from
  *  the client to receive and process it's registration
  *  request
  *
  *  connectionSocket must still be valid because the
  *  response is sent after processing the message (i.e. semantics)
  *
  *  We don't check receive buffer underlying connectionSocket here
  *  because we already have a command and only need to parse it. 
  *  At this juncture we don't care about new commands from the client.
  *  We only care about sending a response to the message we currently
  *  staged.
  */

  State res = GET_REQUEST;

  if (connectionSocket) 
  {
    parseLine(msgFromClient);                        //parse it  

    if (processMessage(tokens) == MSG_REGISTER)       //set registration from tokens
    {
      connectionSocket.println("registered");
      res = SENSE_RFID;
    } 
    else
    {
      connectionSocket.println("unknown");
    }

    /****
      * Close the connectionSocket.  We only allow for a single
      * registration request per connection.
      **/
    connectionSocket.stop();
  }

  return res;
}

/*
Senses using the RFID scanner, will send a response to the server if the scanning was
successful and there is still a connection between the server and arduino
*/
State SenseRFID()
{
  Serial.println("Sensing");

  uint8_t success;

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) 
  {
    if(DEBUG)
    {
      // Display some basic information about the card
      Serial.println("Found an ISO14443A card");
      Serial.print("  UID Length: ");\
      Serial.print(uidLength, DEC);
      Serial.println(" bytes");
      Serial.print("  UID Value: ");
      nfc.PrintHex(uid, uidLength);
      Serial.println("");
    }
  }
  else
  {
    return SENSE_RFID;
  }

  if(!clientRequest)
  {
    return GET_REQUEST;
  }

  /***
    * Only send event if during active period
    */
  if ( (currentTime > eventStartTime) && (currentTime < eventStopTime) ) 
  {    //if within active period
    /***
      * Only fire event once within each delta-t period
      * this is the same as checking if timer has exceeded
      * delta-t since the last event trigger
      */
    Serial.println("event fire within active period");

    if (currentTime >= (lastFireTime + contactDelta)) 
    {
      Serial.println("signaling event fire currentTime >= lastFireTime + contactDelta");
    } 
    else 
    {
      Serial.println("signaling hold event fire");
    } 
  } 
  else if ( currentTime < eventStartTime ) 
  { 
    /****
      * event not active but still valid
      **/
  } 
  else 
  {  //(currentTime > eventStopTime) {
    clientRequest= false;
    return GET_REQUEST;
  } 
  
  return SEND_RESPONSE;
}

/*
Sends event to python server consisting of what RFID has been scanned
*/
State SendResponse()
{
  if (notifier.connect(contactIP, contactPort)) 
  {
    Serial.println("connected");
    // char buffer[uidLength];
    // itoa(uid*, buffer, 10);
    //char* buffer[uidLength];
    
    String message = "";

    for(int i = 0; i < uidLength; i++)
    {
      message += String(uid[i]);

      if(i != uidLength - 1)
      {
        message += String(":");
      }
    }

    //String message = String(buffer);

    notifier.print(message.c_str());   //GFH Proj#7 modification now print, 
                                      // was notifier.println (changed so no newline just message)
    Serial.print("sent message:  ");
    Serial.println(message.c_str());
    notifier.stop();
    lastFireTime= millis();

    return VALIDATE;
  }
  else 
  {
    Serial.print("unable to connect to ");
    Serial.print(contactIP);
    Serial.print(":");
    Serial.println(contactPort);
    notifier.stop();

    clientRequest = false;
    return GET_REQUEST;
  }
}

/*
Waits for a response from the server, 
if there is none after a certain amount of time then it returns to sensing with the RFID,
if the arduino receives a NON_VALID response, it will go back to sensing with the RFID,
if the arduino receives a VALID response, it will continue to unlock the locker
*/
State Validate()
{
  /***
  *  Accept registration message from client
  *  If the registration is received then
  *  transition to parsing the request,
  *  otherwise go back to sensor query.
  *
  *  1.  Check if connectionSocket is valid.
  *  2.  Check if client on other end of 
  *      connection socket has sent data.
  *      This is done by checking if receive buffer
  *      has data (i.e. nonzero byte count).  
  *      Don't wait if the client has nothing to say
  *      at the moment.  The buffer will hang around
  *      and eventually fill until connectionSocket 
  *      is torn down or closed.
  *
  *  Note:  the registration must be parsed
  *         before sending response to client
  */
  connectionSocket = server.available();

  if (connectionSocket)
  {
    if (connectionSocket.available()) 
    {
      validationMsg = fetchLine(connectionSocket);  //retrieve message from client  
      
      if(validationMsg.equals("VALID"))
      {
        elapsedValidationTime = 0;
        Serial.println("Valid: Unlocking");
        return UNLOCK;
      }
      else// if(validationMsg.equals("NOT_VALID"))
      {
        elapsedValidationTime = 0;
        Serial.println("Not Valid");
        return SENSE_RFID;
      }
    }

    connectionSocket.stop();
  } 

  elapsedValidationTime += 1;

  if(elapsedValidationTime >= VALIDATION_TIME)
  {
    elapsedValidationTime = 0;

    if(DEBUG)
    {
      Serial.println("No response given");
    }

    return SENSE_RFID;
  }

  return VALIDATE;
}

/*
Use buzzer to beep once, simulating the opening of a door
*/
State Unlock()
{
  door.write(180);
  return SENSE_CLOSE;
}

/*
Wait for the press of the button, once pressed lock the locker
*/
State SenseClose()
{
  while(digitalRead(BUTTON_PIN) == HIGH)
  {
    ;
  }

  return LOCK;
}

/*
Use buzzer to beep twice, simulating the closing of a door
*/
State Lock()
{
  door.write(0);
  return SENSE_RFID;
}

/*******************
 * Print status of WiFi connection to the
 * Serial terminal
 **/
void printWifiStatus() {
  // if(!DEBUG)
  // {
  //   return;
  // }
  
  // print the SSID of the network you're attached to:

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  ip = WiFi.localIP();

  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();

  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.print("The IP Address is:  ");
  Serial.println(ip);
}

void printStateVars() 
{
  if (!DEBUG)  
  {
    return;
  }
  
  // Serial.print("clientRequest = ");
  // Serial.println(clientRequest);

  // Serial.print("activeRegistration = ");
  // Serial.println(activeRegistration);

  // Serial.print("readyToFire = ");
  // Serial.println(readyToFire);
}

/***
 *  A message is defined as
 *
 *  register IP_address,port,measurement,transition,start,delta,duration
 *  
 *   register:  keword register
 *   IP_address:  string IP_Address
 *   port:   string port number
 *   transition:  rising/falling
 *   duration:  integer
 */
void parseLine(String msg)  
{
  int index= 0;
  char *ptr= NULL;

  if (DEBUG) 
  {
    Serial.println("parseLine");
  }

  /*****
   * Note, the assumption is that the message is never longer than
   * MSG_BUFFLEN
   */
  msg.toCharArray(messageBuffer, MSG_BUFFLEN);

  /*****
   * Intantiate a string tokenizer to parse line of client request
   */

  ptr = strtok(messageBuffer, MSG_DELIMITERS);

  while(ptr != NULL)  
  {
     if (DEBUG) 
     {
       Serial.print("token: ");
       Serial.println(ptr);
     }

     tokens[index] = ptr;
     index++;
     ptr = strtok(NULL, MSG_DELIMITERS);
  }
}

/*******
 *  fetchLine
 *
 *  Retrieve a line of text from the client
 *  Given connection socket, retrief from the client a
 *  command.  By design, a command is terminated by a newline.
 *
 *  connectionSocket- active socket conneting you to the client
 */
String fetchLine(WiFiClient connectionSocket) 
{
  Serial.println("fetchLine");
  /***
    * As long as connection to client maintained
    * fetch line of text from client.
    */
  Serial.print("recieved from client:  ");
  
  //String msgFromClient = "";
  bool lineDone = false;
  String msg = "";
  
  while( connectionSocket.connected() && (!lineDone) ) 
  {
    if (connectionSocket.available() && (!lineDone) ) 
    {
        char c = connectionSocket.read();

        Serial.write(c);

        if (c != '\n') 
        {
          msg += c;
        }  
        else 
        {
          lineDone= true;
          break;
        }
    }
  }

  return msg;
}

/***
 *  processMessage
 *
 *  Use parsed tokens to set registration information.
 *  Tokens are assumed populated by the parseLine routine, 
 *  that is an array of char * (strings) in order 
 * 
 *  1. registration command:  { register }
 *  2. contact/delivery IP:   validIP
 *  3. contact/delivery port:  integer
 *  4. measurement type:   { temperature, humidity }
 *  5. transition:  { rising,falling, any}
 *  6. start time:  double (millis)
 *  7. delta time:  double (millis)
 *  8. duration:  double (millis)
 */
int processMessage(char** toks) 
{
   int msgType= MSG_NONE;
   int index= 0;

   String command(toks[index]);    //registration commend
   index++;

   String ipStr(toks[index]);      //IP address string
   index++;

   String port(toks[index]);       //port number string
   index++;

   String measurement(toks[index]); //measurement type
   index++;

   String eventStr(toks[index]);    //rising or falling or any
   index++;

   String start(toks[index]);       //start time 
   index++;

   String delta(toks[index]);       //delta time
   index++;

   String durationStr(toks[index]); //duration



   /***
    * Implementation of client request semantics
    *
    * At the moment, it only implements event registrations
    */
   if (command.equals("register")) 
   {
      if (DEBUG)  
      {
        Serial.print("processMessage: command = ");
        Serial.println(command);
      }

      if (!contactIP.fromString(ipStr.c_str())) 
      {   //get the IP address
        return MSG_NONE;
      }    

      contactPort = port.toInt();                    //get the port number
      
      if (eventStr.equals("scanning"))           
      {          //set the event transition type
        contactEvent = SCANNER;                  
      } 
      else 
      {
        contactEvent = NONE;
      }

      contactStart = (unsigned long) start.toDouble();
      contactDelta = (unsigned long) delta.toDouble();
      contactDuration = (unsigned long) durationStr.toDouble();

      eventStartTime = beginOfTime + contactStart;
      eventStopTime = beginOfTime + contactStart + contactDuration;

      msgType = MSG_REGISTER;
   }

   if (DEBUG)  
   {
     Serial.print("processMessage: msgType= ");
     Serial.println(msgType);
   }

   return msgType;
}