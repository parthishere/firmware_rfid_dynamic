#include <Arduino.h>
#include <ArduinoJson.h>
#include <MFRC522.h> //MFRC522 library
#include "BluetoothSerial.h"
#include <Wire.h>
#include <base64.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>

#if !(defined(ESP8266) || defined(ESP32))
#error This code is intended to run on the ESP8266 or ESP32 platform! Please check your Tools->Board setting.
#endif

#define ASYNC_HTTPS_REQUEST_GENERIC_VERSION_MIN_TARGET "AsyncHTTPSRequest_Generic v2.2.1"
#define ASYNC_HTTPS_REQUEST_GENERIC_VERSION_MIN 2002001

/////////////////////////////////////////////////////////

// Uncomment for certain HTTP site to optimize
// #define NOT_SEND_HEADER_AFTER_CONNECTED        true

// Level from 0-4
#define ASYNC_HTTPS_DEBUG_PORT Serial

#define _ASYNC_TCP_SSL_LOGLEVEL_ 1
#define _ASYNC_HTTPS_LOGLEVEL_ 1

// 300s = 5 minutes to not flooding
#define HTTPS_REQUEST_INTERVAL 60 // 300

// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#include <AsyncHTTPSRequest_Generic.h>

#define SDA_SS_PIN 5 // 21 //ESP Interface Pin
#define RST_PIN 15   // 22    //ESP Interface Pin

#define bluetooth_switch 14

#define BLUE 27   // mode on button press or not // continuous means mode 1 blinking means mode 2
#define BLUE2 26  // data available or not
#define YELLOW 12 // error card not detected
#define WHITE 13  // write sucsess full
#define RED 21    // wifi
#define RED2 25   // server error
#define BUZZER 18
#define BATTERY_SENSE_PIN 39
#define BATTERY_LEVEL_INDECATOR 26

#define MODE_SW1 32
#define MODE_SW2 35

#define ID_MIN_EEPROM_ADDRESS 226
#define ID_MAX_EEPROM_ADDRESS 230

#define SIZE_BUFFER 18
#define MAX_SIZE_BLOCK 16

#define TIME_TO_CHECK_FOR_BATTERY 1000 * 60

MFRC522 mfrc522(SDA_SS_PIN, RST_PIN); // create instance of class
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;
BluetoothSerial btSerial;
AsyncHTTPSRequest request;

struct rfidData
{
  String uid;
  String data;
};
rfidData rfidStruct;
WebServer server(80);

/* Set the block to which we want to write data */
/* Be aware of Sector Trailer Blocks */
int blockNum = 2;

/* Create an array of 16 Bytes and fill it with data */
/* This is the actual data which is going to be written into the card */
/* Create another array to read data from Block */
/* Length of buffer should be 2 Bytes more than the size of Block (16 Bytes) */

byte bufferLen = 18;
byte readBlockData[18];

// Variables

String ssid_pass; // character and character string buffer
String uqid = "";
String username = "";
String password = "";

String st;
String content;
String esid;
String epass = "";
int statusCode;

String val_to_write_from_get = "";

long last_time_recived_data, last_time_checked_battery{1000 * 61};
const char *AuthenticatedTag = "E3E7719B";

static int id;

// Function Decalration

bool checkWifi_connection(String username, String password, String uqid);
void send_data_to_bt_and_setup_sta(void);
String readUid();
rfidData readingData();
rfidData writingData(String write_data);
void launchWeb(void);
void setupAP(void);
void createWebServer();

void sendRequest()
{
  btSerial.end();
  static bool requestOpenResult;
  if (request.readyState() == readyStateUnsent || request.readyState() == readyStateDone)
  {

    String temp_url = "https://fleetkaptan.up.railway.app/api/rfid/" + uqid + "/" + username + "/write-to-rfid/";
    requestOpenResult = request.open("GET", temp_url.c_str());
    if (requestOpenResult)
    {
      // Only send() if open() returns true, or crash
      String input_s = username + ":" + password;
      String encoded = base64::encode(input_s);
      String auth = "Basic " + encoded;
      request.setReqHeader("Authorization", auth.c_str());
      request.send();
    }
    else
    {
      Serial.println(F("Can't send bad request"));
    }
  }
  else
  {
    Serial.println(F("Can't send request"));
  }
  // btSerial.begin(9600);
}

void requestCB(void *optParm, AsyncHTTPSRequest *request, int readyState)
{
  (void)optParm;

  if (readyState == readyStateDone)
  {
    StaticJsonDocument<1024> jsonDocument;
    DeserializationError error;
    if (request->responseHTTPcode() == 200)
    {
      Serial.println(F("\n**************************************"));
      String response = request->responseText();
      Serial.println(response);
      Serial.println(F("**************************************"));

      response.trim();
      try
      {
        error = deserializeJson(jsonDocument, response);

        if (error)
        {
          Serial.print("Deserialization error: ");
          Serial.println(error.c_str());
          return;
        }

        digitalWrite(RED2, LOW);

        int id = jsonDocument["id"];
        const char *unique_id_recived_from_server = jsonDocument["esp"]["unique_id"];
        const char *value_recived_from_server = jsonDocument["value"];
        bool sent_from_server = jsonDocument["sent_from_server"];
        Serial.println(sent_from_server);

        unsigned int resultID = 0;
        for (int i = 0; i < sizeof(unsigned int); i++) {
            resultID |= EEPROM.read(ID_MIN_EEPROM_ADDRESS + i) << (8 * i);
        }
        Serial.println("this is the result ID");
        Serial.println(resultID);


        if (id != resultID && strcmp(uqid.c_str(), unique_id_recived_from_server) == 0 && sent_from_server)
        {
          val_to_write_from_get = String(value_recived_from_server);
          val_to_write_from_get = val_to_write_from_get.substring(0, 16);
          for (int i=ID_MIN_EEPROM_ADDRESS;i<=ID_MAX_EEPROM_ADDRESS; i++){
            EEPROM.write(i, 0);
          }
          EEPROM.commit();
          for (int i = 0; i < sizeof(unsigned int); i++) {
              EEPROM.write(ID_MIN_EEPROM_ADDRESS + i, ((id >> (8 * i)) & 0xFF)) ;
          }
          EEPROM.commit();
        }
      }
      catch (...)
      {
        Serial.println("error occured");
      }
    }

    request->setDebug(false);
  }
}

void post_data(String D0, String data, String uid)
{
  btSerial.end();

  StaticJsonDocument<1024> jsonDocument;
  DeserializationError error;

  String input_s = username + ":" + password;
  String encoded = base64::encode(input_s);
  String auth = "Basic " + encoded;

  WiFiClientSecure client;
  client.setInsecure();

  if (client.connect("fleetkaptan.up.railway.app", 443) && WiFi.status() == WL_CONNECTED)
  {

    String this_will_be_sent_to_server_hehe = "D0=" + String(D0) + "&data=" + String(data) + "&uid=" + String(uid);

    client.println("POST /api/rfid/" + uqid + "/" + username + "/read-esp-scanned HTTP/1.1");
    client.println("Host: fleetkaptan.up.railway.app");
    client.println("User-Agent: ESP32");
    client.println("Authorization: " + auth);
    client.println("Content-Type: application/x-www-form-urlencoded;");
    client.println("Content-Length: " + String(this_will_be_sent_to_server_hehe.length()));
    client.println();
    client.println(this_will_be_sent_to_server_hehe);

    while (client.connected())
    {
      String line = client.readStringUntil('\n');
      if (line == "\r")
      {
        Serial.println("headers received");
        break;
      }
    }

    String response;
    if (client.available())
    {
      response = client.readString();
    }
    response.trim();
    Serial.println(response);
    client.stop();

    error = deserializeJson(jsonDocument, response);

    if (error)
    {
      Serial.print("Deserialization error: ");
      Serial.println(error.c_str());
      return;
    }

    digitalWrite(RED2, LOW);
    // // Access the JSON data
    // if (jsonDocument.containsKey("id") && !jsonDocument["id"].isNull())
    // {

    //   int id = jsonDocument["id"];
    //   const char *uniqueId = jsonDocument["esp"]["unique_id"];
    //   const char *value = jsonDocument["value"];
    // }
    // else
    // {
    //   Serial.println("ID not found or null");
    // }

    client.stop();
  }
  else
  {
    Serial.println(F("Connection wasnt established"));
    digitalWrite(RED2, HIGH);
  }
  // btSerial.begin(9600);
}

void setup()
{

  Serial.begin(9600);
  delay(100);
  btSerial.begin(9600);
  delay(10);

  Serial.println("Ready");

  pinMode(bluetooth_switch, INPUT_PULLUP);

  pinMode(MODE_SW1, INPUT_PULLUP);
  pinMode(MODE_SW2, INPUT_PULLUP);

  pinMode(WHITE, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(BLUE2, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(RED2, OUTPUT);
  pinMode(BATTERY_LEVEL_INDECATOR, OUTPUT);
  pinMode(BATTERY_SENSE_PIN, INPUT);

  EEPROM.begin(512); // Initialasing EEPROM
  delay(200);

  Serial.println("Reading EEPROM ssid");

  String esid;
  for (int i = 0; i < 32; ++i)
  {
    char c = char(EEPROM.read(i));
    if (c != 0)
    {
      esid += c;
    }
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("");
  Serial.println("Reading EEPROM pass");

  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    char c = char(EEPROM.read(i));
    if (c != 0)
    {
      epass += c;
    }
  }
  Serial.print("PASS: ");
  Serial.println(epass);
  Serial.println("");
  Serial.println("Reading EEPROM username");

  for (int i = 96; i < 128; ++i)
  {
    char c = char(EEPROM.read(i));
    if (c != 0)
    {
      username += c;
    }
  }
  Serial.print("USERNAME: ");
  Serial.println(username);
  Serial.println("");
  Serial.println("Reading EEPROM PASSWORD");

  for (int i = 128; i < 192; ++i)
  {
    char c = char(EEPROM.read(i));
    if (c != 0)
    {
      password += c;
    }
  }
  Serial.print("PASSWORD: ");
  Serial.println(password);
  Serial.println("");
  Serial.println("Reading EEPROM UQID");

  for (int i = 196; i < 224; ++i)
  {
    char c = char(EEPROM.read(i));
    if (c != 0)
    {
      uqid += c;
    }
  }
  Serial.print("uqid: ");
  Serial.println(uqid);

  delay(100);

  username.trim();
  uqid.trim();
  password.trim();

  WiFi.begin(esid.c_str(), epass.c_str());
  if (checkWifi_connection(uqid, username, password))
  {
    Serial.println("Succesfully Connected!!!");
    digitalWrite(RED, 0);
    SPI.begin(); // Initiate  SPI bus
    mfrc522.PCD_Init();
    mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  }
  else
  {
    digitalWrite(RED, 1);
    while (WiFi.status() != WL_CONNECTED)
    {

      // Setup Bluetooth
      if (digitalRead(bluetooth_switch) == LOW)
      {
        Serial.print("State of switch");
        Serial.print(digitalRead(bluetooth_switch));

        digitalWrite(BLUE, HIGH);
        Serial.println("Turning the Bluetooth On");
        send_data_to_bt_and_setup_sta();
      }
      else
      {
        digitalWrite(BLUE, 0);
        Serial.println("Connection Status Negative");
        Serial.println("Turning the HotSpot On");
        launchWeb();
        setupAP(); // Setup HotSpot
        Serial.println("Do nothing");
        while ((WiFi.status() != WL_CONNECTED) && digitalRead(bluetooth_switch) == HIGH)
        {
          delay(10);
          server.handleClient();
        }
      }
    }
  }

  Serial.println();
  Serial.println("Waiting...");

  request.setDebug(false);

  request.onReadyStateChange(requestCB);

  for (int i = 0; i < 3; i++)
  {
    digitalWrite(WHITE, HIGH);
    digitalWrite(YELLOW, HIGH);
    digitalWrite(BLUE, HIGH);
    digitalWrite(BLUE2, HIGH);
    digitalWrite(RED, HIGH);
    digitalWrite(RED2, HIGH);
    delay(100);
    digitalWrite(WHITE, LOW);
    digitalWrite(YELLOW, LOW);
    digitalWrite(BLUE, LOW);
    digitalWrite(BLUE2, LOW);
    digitalWrite(RED, LOW);
    digitalWrite(RED2, LOW);
    delay(100);
  }
}

void loop()
{
  if (millis() - last_time_recived_data > 7000)
  {
    Serial.println("wil get");
    sendRequest();
    last_time_recived_data = millis();
  }

  if (millis() - last_time_checked_battery > TIME_TO_CHECK_FOR_BATTERY)
  {
    Serial.println("wil check battery");
    uint16_t bat_value = analogRead(BATTERY_SENSE_PIN);
    Serial.println("BATTERY READ");
    Serial.println(bat_value);
    Serial.println();
    if (bat_value < 4096 && bat_value > (4096 - 1024))
    {
      digitalWrite(BATTERY_LEVEL_INDECATOR, HIGH);
      delay(5000);
      digitalWrite(BATTERY_LEVEL_INDECATOR, LOW);
      
    }
    else if (bat_value < (4096 - 1024) && bat_value > (4096 - (2 * 1024)))
    {
      digitalWrite(BATTERY_LEVEL_INDECATOR, HIGH);
      delay(2500);
      digitalWrite(BATTERY_LEVEL_INDECATOR, LOW);
      delay(2500);
    }
    else if (bat_value < (4096 - (2 * 1024)) && bat_value > (4096 - (3 * 1024)))
    {
      digitalWrite(BATTERY_LEVEL_INDECATOR, HIGH);
      delay(900);
      digitalWrite(BATTERY_LEVEL_INDECATOR, LOW);
      delay(900);
      digitalWrite(BATTERY_LEVEL_INDECATOR, HIGH);
      delay(900);
      digitalWrite(BATTERY_LEVEL_INDECATOR, LOW);
      delay(900);
    }
    else
    {
      for (int i=0; i<8; i++){
        digitalWrite(BATTERY_LEVEL_INDECATOR, HIGH);
        delay(100);
        digitalWrite(BATTERY_LEVEL_INDECATOR, LOW);
        delay(100);
      }
    }
    last_time_checked_battery = millis();
  }
  if (val_to_write_from_get != "")
  {
    // digitalWrite(BLUE2, HIGH);
    Serial.println("changes");

    {
      rfidData rfid = writingData(val_to_write_from_get);

      while (strcmp(rfid.uid.c_str(), "") == 0)
      {
        digitalWrite(YELLOW, HIGH);
        rfid = writingData(val_to_write_from_get);
        vTaskDelay(pdMS_TO_TICKS(500));
      }
    }
    digitalWrite(YELLOW, LOW);

    for (int i = 0; i < 4; i++)
    {
      digitalWrite(WHITE, HIGH);
      delay(100);
      digitalWrite(WHITE, LOW);
      delay(100);
    }

    digitalWrite(BLUE2, LOW);
    val_to_write_from_get = "";

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  if (digitalRead(bluetooth_switch) == LOW)
  {
    delay(100);
    btSerial.end();
    request.abort();
    delay(100);
    btSerial.begin(9600);
    delay(100);

    while (digitalRead(bluetooth_switch) == LOW)
    {
      digitalWrite(BLUE, HIGH);

      while (btSerial.available())
      {
        digitalWrite(BLUE, HIGH);
        ssid_pass = btSerial.readString();

        String data_to_write_from_bt = ssid_pass;
        Serial.println(ssid_pass);

        String dataToWrite = ssid_pass; // Example string to write
        dataToWrite = dataToWrite.substring(0, 16);
        {
          rfidData rfid = writingData(dataToWrite);
          while (strcmp(rfid.uid.c_str(), "") == 0)
          {
            digitalWrite(YELLOW, HIGH);
            rfid = writingData(dataToWrite);
            delay(1000);
          }
          Serial.println(rfid.uid);

          if (strcmp(rfid.uid.c_str(), "") != 0)
          {
            Serial.println(rfid.uid);
            post_data("0", rfid.data, rfid.uid);
          }
        }

        digitalWrite(YELLOW, LOW);
        for (int i = 0; i < 4; i++)
        {
          digitalWrite(WHITE, HIGH);
          delay(100);
          digitalWrite(WHITE, LOW);
          delay(100);
        }

        digitalWrite(BLUE, LOW);
        break;

        // write rifd
      }
    }
    btSerial.end();
  }

  digitalWrite(BLUE, 0);
  {
    rfidData rfid = readingData();

    if (strcmp((const char *)rfid.uid.c_str(), "") != 0)
    {
      Serial.println("card readed");
      for (int i = 0; i < 2; i++)
      {
        digitalWrite(WHITE, HIGH);
        delay(1000);
        digitalWrite(WHITE, LOW);
        delay(500);
      }

      Serial.println("will post");
      post_data("0", rfid.data, rfid.uid);
    }
  }
}

bool checkWifi_connection(String username, String password, String uqid)
{
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while (c < 25)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      if (username.length() == 0 && password.length() == 0 && uqid.length() == 0)
      {
        return false;
      }
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connection timed out, password or username are incorrect");
  return false;
}

void send_data_to_bt_and_setup_sta(void)
{

  while (digitalRead(bluetooth_switch) == LOW)
  {

    while (btSerial.available())
    {
      Serial.println("data recived");
      ssid_pass = btSerial.readString();
      String ssid_pass_2 = ssid_pass;
      Serial.println(ssid_pass);
      char *token;
      token = strtok((char *)ssid_pass_2.c_str(), ",");

      // Loop through the remaining tokens
      int count = 0;
      while (token != NULL)
      {
        printf("%s\n", token);
        ;

        switch (count)
        {
        case 0:
          Serial.println("Clearing EEPROM");
          for (int i = 0; i < 256; ++i)
          {
            EEPROM.write(i, 0);
          }
          Serial.println("writing eeprom ssid:");
          for (int i = 0; i < strlen(token); ++i)
          {
            EEPROM.write(i, token[i]);
            Serial.print("Wrote: ");
            Serial.println(token[i]);
          }
          break;
        case 1:
          Serial.println("writing eeprom pass:");
          for (int i = 0; i < strlen(token); ++i)
          {
            EEPROM.write(32 + i, token[i]);
            Serial.print("Wrote: ");
            Serial.println(token[i]);
          }
          break;
        case 2:
          Serial.println("writing eeprom uqid:");
          Serial.println(token);
          for (int i = 0; i < strlen(token); ++i)
          {
            EEPROM.write(196 + i, token[i]);
            Serial.print("Wrote: ");
            Serial.println(token[i]);
          }

          break;
        case 3:
          Serial.println("writing eeprom username:");
          for (int i = 0; i < strlen(token); ++i)
          {
            EEPROM.write(96 + i, token[i]);
            Serial.print("Wrote: ");
            Serial.println(token[i]);
          }
          break;
        case 4:
          Serial.println("writing eeprom passw3ord:");
          for (int i = 0; i < strlen(token); ++i)
          {
            EEPROM.write(128 + i, token[i]);
            Serial.print("Wrote: ");
            Serial.println(token[i]);
          }
          EEPROM.commit();
          delay(100);
          ESP.restart();
          break;

        default:
          Serial.println("do nothig");
          break;
        }
        token = strtok(NULL, ",");
        count++;
      }
    }
  }
}

String readUid()
{
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return "";
  }
  // Select a card
  if (!mfrc522.PICC_ReadCardSerial())
  {
    return "";
  }
  // prints the technical details of the card/tag
  mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));

  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    // debug(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : "");
    // debug(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : ""));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  Serial.println("");
  content.toUpperCase();
  char *idTag = new char[content.length() + 1];
  strcpy(idTag, content.c_str());

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  return String(idTag);
}
// reads data from card/tag

rfidData readingData()
{
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    rfidStruct.uid = "";
    rfidStruct.data = "";

    return rfidStruct;
  }
  // Select a card
  if (!mfrc522.PICC_ReadCardSerial())
  {
    rfidStruct.uid = "";
    rfidStruct.data = "";

    return rfidStruct;
  }
  // prints the technical details of the card/tag
  mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));

  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    // debug(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : "");
    // debug(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : ""));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  Serial.println("");
  content.toUpperCase();
  char *idTag = new char[content.length() + 1];
  strcpy(idTag, content.c_str());

  delay(100);

  // prepare the key - all keys are set to FFFFFFFFFFFFh
  for (byte i = 0; i < 6; i++)
    key.keyByte[i] = 0xFF;

  // buffer for read data
  byte buffer[SIZE_BUFFER] = {0};

  // the block to operate
  byte block = 1;
  byte size = SIZE_BUFFER;                                                                            // authenticates the block to operate
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid)); // line 834 of MFRC522.cpp file
  if (status != MFRC522::STATUS_OK)
  {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    rfidStruct.uid = "";
    rfidStruct.data = "";

    return rfidStruct;
  }

  // read data from block
  status = mfrc522.MIFARE_Read(blockNum, buffer, &size);
  if (status != MFRC522::STATUS_OK)
  {
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    rfidStruct.uid = "";
    rfidStruct.data = "";

    return rfidStruct;
  }

  Serial.print(F("\nData from block ["));
  Serial.print(block);
  Serial.print(F("]: "));

  for (uint8_t i = 0; i < MAX_SIZE_BLOCK; i++)
  {
    Serial.write(buffer[i]);
  }

  Serial.println(" ");

  mfrc522.PICC_HaltA();
  // "stop" the encryption of the PCD, it must be called after communication with authentication, otherwise new communications can not be initiated
  mfrc522.PCD_StopCrypto1();

  String dataString((char *)buffer);
  // Serial.println("two lines");
  // Serial.println(String(idTag));
  // Serial.println(dataString);
  rfidStruct.uid = String(idTag);
  rfidStruct.data = dataString;
  content.clear();
  return rfidStruct;
}

rfidData writingData(String write_data)
{

  Serial.println("1");
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    rfidStruct.uid = "";
    rfidStruct.data = "";

    return rfidStruct;
  }
  // Select a card
  Serial.println("2");
  if (!mfrc522.PICC_ReadCardSerial())
  {
    rfidStruct.uid = "";
    rfidStruct.data = "";

    return rfidStruct;
  }

  // prints thecnical details from of the card/tag
  Serial.println("3");
  mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));

  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    // debug(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : "");
    // debug(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : ""));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  Serial.println("");
  content.toUpperCase();
  char *idTag = new char[content.length() + 1];
  strcpy(idTag, content.c_str());

  for (byte i = 0; i < 6; i++)
    key.keyByte[i] = 0xFF;

  // String dataToWrite = "Hello, RFID!";  // Example string to write
  byte data[MAX_SIZE_BLOCK]; // Array to hold 16 bytes of data

  memset(data, 0, sizeof(data)); // Clear the data array
  memcpy(data, write_data.c_str(), min(write_data.length(), sizeof(data)));

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
                                    blockNum, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK)
  {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));

    rfidStruct.uid = "";
    rfidStruct.data = "";

    return rfidStruct;
  }
  // else Serial.println(F("PCD_Authenticate() success: "));

  // Writes in the block
  status = mfrc522.MIFARE_Write(blockNum, data, MAX_SIZE_BLOCK);

  mfrc522.PICC_HaltA();
  // "stop" the encryption of the PCD, it must be called after communication with authentication, otherwise new communications can not be initiated
  mfrc522.PCD_StopCrypto1();

  if (status != MFRC522::STATUS_OK)
  {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    rfidStruct.uid = "";
    rfidStruct.data = "";

    return rfidStruct;
  }
  else
  {
    Serial.println(F("MIFARE_Write() success: "));
    rfidStruct.uid = idTag;
    rfidStruct.data = write_data;

    return rfidStruct;
  }
}

void launchWeb()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.println("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());

  delay(1000);
  createWebServer();
  // Start the server
  ///////////////////
  server.begin(); //
  ///////////////////
  Serial.println("Server started");
}

void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.println(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.println(i + 1);
      Serial.println(": ");
      Serial.println(WiFi.SSID(i));
      Serial.println(" (");
      Serial.println(WiFi.RSSI(i));
      Serial.println(")");
      // Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
    st += ")";
    // st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  WiFi.softAP("FleetKaptan", "");
  Serial.println("Initializing_softap_for_wifi credentials_modification");
  launchWeb();
  Serial.println("over");
}

void createWebServer()
{
  {
    server.on("/", []()
              {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Welcome to Wifi Credentials Update page";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += ipStr;
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><label>UQID: </label><input name='uqid' length=64><label>Website Username: </label><input name='wusername' length=64><label>Password: </label><input name='wpass' length=64><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content); });
    server.on("/scan", []()
              {
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content); });
    server.on("/setting", []()
              {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      String uqid = server.arg("uqid");
      String wusername = server.arg("wusername");
      String wpass = server.arg("wpass");

      if (qsid.length() > 0 && qpass.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        }
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");
        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          Serial.println("Wrote: ");
          Serial.println(qsid[i]);
        }
        Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          Serial.println("Wrote: ");
          Serial.println(qpass[i]);
        }
        for (int i = 0; i < uqid.length(); ++i)
        {
          EEPROM.write(196 + i, uqid[i]);
          Serial.print("Wrote: ");
          Serial.println(uqid[i]);
        }
        for (int i = 0; i < wusername.length(); ++i)
        {
          EEPROM.write(96 + i, wusername[i]);
          Serial.print("Wrote: ");
          Serial.println(wusername[i]);
        }
        for (int i = 0; i < wpass.length(); ++i)
        {
          EEPROM.write(128 + i, wpass[i]);
          Serial.print("Wrote: ");
          Serial.println(wpass[i]);
        }
        EEPROM.commit();
        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        ESP.restart();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content); });
  }
}
