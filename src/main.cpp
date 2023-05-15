#include <Arduino.h>

#include "Fligree.h"
#include <ArduinoJson.h>
#include <MFRC522.h> //MFRC522 library
#include <HTTPClient.h>
#include "BluetoothSerial.h"
#include <Wire.h>


#define SDA_SS_PIN 5 // 21 //ESP Interface Pin
#define RST_PIN 15   // 22    //ESP Interface Pin

MFRC522 mfrc522(SDA_SS_PIN, RST_PIN); // create instance of class
MFRC522::MIFARE_Key key;

char* UNIQUE_ID = "aaaaa";
#define USERNAME "parth"

const size_t MAX_CONTENT_SIZE = 512;

#define max_timeout 100

#define url "http://192.168.1.123:8000/"


// unsigned long counterChannelNumber = 1922816;                // Channel ID
// const char * myCounterReadAPIKey = "2MLZDDQIBA7S319S";      // Read API Key
// const int FieldNumber1 = 1;

int sw = 14, IC = 0, ir=12;
const int LED1 = 1;
//Variables

String pin_D0, pin_D1, pin_D2, pin_D3, pin_D4, pin_A0;
int statusCode;
unsigned int time_send_scanned_network;
const char* ssid = "text";
const char* passphrase = "text";
char* cString;
String st, another_st, new_ssid_from_bt, new_pass_from_bt;
String content, ssid_pass;
String payload, code;
char  B, buff[1000]; // character and character string buffer
int  buffCount = 0; // how many characters in the buffer
int  buffCharLimit = 1000; // maximum characters allowed


String uqid = "";
String username = "";
String password = "";

String ENDPOINT_DATA_TO_SERVER = "api/rfid/"+uqid+"/"+USERNAME+"/read-esp-scanned";
String ENDPOINT_DATA_FROM_SERVER = "api/rfid/"+uqid+"/"+USERNAME+"/write-to-rfid";

//Function Decalration
bool checkWifi_connection(String username, String password, String uqid);
void func(String payload);
void createWebServer();
void send_data_to_bt_and_setup_sta(void);
void end_line_and_print(void);
void manage_bt_data(void);


Fligree esp;
BluetoothSerial btSerial;
WiFiClient client;



void setup() {

  
  
  Serial.begin(9600);
  Serial.println("hey");
  delay(100);
  btSerial.begin(9600);
  delay(10);

  Serial.println("Ready");
 
  pinMode(sw, INPUT_PULLUP);
  // pinMode(ir, INPUT);

  EEPROM.begin(512);  //Initialasing EEPROM
  delay(200);

  
  // Serial.println("Enter 1 for cleaing eeprom");
  // while (millis() < max_timeout * 100) {
  //   IC = Serial.parseInt();
  //   Serial.print("!");
  //   delay(300);
  // }
  // Serial.println("Debuging time over... ");


  // if (IC == 1) {
    
  //   Serial.println("Clearing EEPROM");
  //   for (int i = 0; i < 256; ++i) {
  //     EEPROM.write(i, 0);
  //   }
  //   delay(10);
  //   Serial.println("Continue");
  // } else {
  //   Serial.println("EEPROM isn't cleared !");
  // }

  // Read EEPROM for SSID and pass
  Serial.println("Reading EEPROM ssid");

  String esid;
  for (int i = 0; i < 32; ++i) {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("");
  Serial.println("Reading EEPROM pass");

  String epass = "";
  for (int i = 32; i < 96; ++i) {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);
  Serial.println("");
  Serial.println("Reading EEPROM username");


  for (int i = 96; i < 128; ++i) {
    username += char(EEPROM.read(i));
  }
  Serial.print("USERNAME: ");
  Serial.println(username);
  Serial.println("");
  Serial.println("Reading EEPROM PASSWORD");

  for (int i = 128; i < 192; ++i) {
    password += char(EEPROM.read(i));
  }
  Serial.print("PASSWORD: ");
  Serial.println(password);
  Serial.println("");
  Serial.println("Reading EEPROM UQID");

  for (int i = 196; i < 224; ++i) {
    uqid += char(EEPROM.read(i));
  }
  

  delay(100);
  
  
  
  WiFi.begin(esid.c_str(), epass.c_str());
  if (checkWifi_connection(uqid, username, password)) {
    Serial.println("Succesfully Connected!!!");
    
    esp.begin(url, username, password, uqid);
    SPI.begin();        // Initiate  SPI bus
    mfrc522.PCD_Init();
    // ThingSpeak.begin(client);
    return;
  } else {
    
    
    // Setup Bluetooth
    Serial.print("State of switch");
    Serial.print(digitalRead(sw));
    if (digitalRead(sw) == LOW) {
      Serial.println("Turning the Bluetooth On");
      send_data_to_bt_and_setup_sta();
    }
    else {
      Serial.println("Do nothing");
    }
  }

  Serial.println();
  Serial.println("Waiting...");

  while ((WiFi.status() != WL_CONNECTED)) {
    delay(1);
    send_data_to_bt_and_setup_sta();
  }

}

long last_time_scaned_rfid;
const char *AuthenticatedTag = "E3E7719B";
void loop() {

    if (millis() - last_time_scaned_rfid > 10000)
  {

    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++)
      key.keyByte[i] = 0xFF;
    byte block;
    byte len;
    MFRC522::StatusCode status;

    if (!mfrc522.PICC_IsNewCardPresent())
    {
      return;
    }

    if (!mfrc522.PICC_ReadCardSerial())
    {
      return;
    }

    Serial.println(F("**Card Detected:**"));

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
    if (strcmp(idTag, AuthenticatedTag) == 0)
    {
      // same tag
      //  we have to set logic for setting up the senind of id tag
      
      last_time_scaned_rfid = millis();
      
    }
    Serial.println(F("\n**End Reading**\n"));
    content = "";
  
  }
  if(digitalRead(sw) == LOW){
    while (btSerial.available())
    {
      ssid_pass = btSerial.readString();
      String ssid_pass_2 = ssid_pass;
      Serial.println(ssid_pass);
      char * token;
    
      }
  }
  // int A = ThingSpeak.readLongField(counterChannelNumber, FieldNumber1, myCounterReadAPIKey);


  
  // if (A == 1){
  //   myservo.write(90);
  // }
  // else{
  //   myservo.write(0);
  // }
  // Serial.println(A);
  // Serial.println(digitalRead(ir));
  // Serial.println(digitalRead(sw));
  // Serial.println();

  // lcd.setCursor(2,1);
  // lcd.print(digitalRead(ir));

  // lcd.setCursor(4,1);
  // lcd.print(digitalRead(sw));
}



bool checkWifi_connection(String username, String password, String uqid) {
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while (c < 20) {
    if (WiFi.status() == WL_CONNECTED) {
      if (username.length() == 0 && password.length() == 0 && uqid.length() == 0) {
        return false;
      }
      return true;
    }
    delay(100);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connection timed out, password or username are incorrect");
  return false;
}



void send_data_to_bt_and_setup_sta(void) {
  
  while (digitalRead(sw) == LOW)
  {
    
    while (btSerial.available())
    {
      ssid_pass = btSerial.readString();
      String ssid_pass_2 = ssid_pass;
      Serial.println(ssid_pass);
      char * token;
      token = strtok((char*)ssid_pass_2.c_str(), ",");

      
    // Loop through the remaining tokens
      int count = 0;
      while (token != NULL) {
          printf("%s\n", token);
          ;
       
          switch(count){
            case 0:
              Serial.println("Clearing EEPROM");
              for (int i = 0; i < 256; ++i) {
                EEPROM.write(i, 0);
              }
              Serial.println("writing eeprom ssid:");
              for (int i = 0; i < strlen(token); ++i) {
                EEPROM.write(i, token[i]);
                Serial.print("Wrote: ");
                Serial.println(token[i]);
              }
              break;
            case 1:
              Serial.println("writing eeprom pass:");
              for (int i = 0; i < strlen(token); ++i) {
                EEPROM.write(32+i, token[i]);
                Serial.print("Wrote: ");
                Serial.println(token[i]);
              }
              break;
            case 2:
              Serial.println("writing eeprom uqid:");
              Serial.println(token);
              for (int i = 0; i < strlen(token); ++i) {
                EEPROM.write(196+i, token[i]);
                Serial.print("Wrote: ");
                Serial.println(token[i]);
              }
              // count = 0;
              // delay(100);
              // EEPROM.commit();
              // delay(100);
              // ESP.restart();
              break;
            case 3:
              Serial.println("writing eeprom username:");
              for (int i = 0; i < strlen(token); ++i) {
                EEPROM.write(96+i, token[i]);
                Serial.print("Wrote: ");
                Serial.println(token[i]);
              }
              break;
             case 4:
              Serial.println("writing eeprom passw3ord:");
              for (int i = 0; i < strlen(token); ++i) {
                EEPROM.write(128+i, token[i]);
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
          count ++;
      }


      // for (int i = 0; i < ssid_pass.length(); i++)
      // {
      //   // if (ssid_pass.substring(i, i + 1) == ",") // you have to send ssid,password just like this 
      //   // {
      //   //   new_ssid_from_bt = ssid_pass.substring(0, i);
      //   //   new_pass_from_bt = ssid_pass.substring(i + 1);
      //   //   Serial.print("SSID = "); Serial.println(new_ssid_from_bt);
      //   //   Serial.print("Password = "); Serial.println(new_pass_from_bt);
      //   //   delay(2000);

      //   //   int n1 = new_ssid_from_bt.length();
      //   //   char char_array1[n1 + 1];
      //   //   strcpy(char_array1, new_ssid_from_bt.c_str());

      //   //   int n2 = new_pass_from_bt.length();
      //   //   char char_array2[n2 + 1];
      //   //   strcpy(char_array2, new_pass_from_bt.c_str());


      //   //   Serial.println("Clearing EEPROM");
      //   //   for (int i = 0; i < 256; ++i) {
      //   //     EEPROM.write(i, 0);
      //   //   }
      //   //   delay(10);
      //   //   Serial.println("Continue");

      //   //   Serial.println("writing eeprom ssid:");
      //   //   for (int i = 0; i < new_ssid_from_bt.length(); ++i) {
      //   //     EEPROM.write(i, new_ssid_from_bt[i]);
      //   //     Serial.print("Wrote: ");
      //   //     Serial.println(new_ssid_from_bt[i]);
      //   //   }

      //   //   Serial.println("writing eeprom pass:");
      //   //   for (int i = 0; i < new_pass_from_bt.length(); ++i) {
      //   //     EEPROM.write(32 + i, new_pass_from_bt[i]);
      //   //     Serial.print("Wrote: ");
      //   //     Serial.println(new_pass_from_bt[i]);
      //   //   }

      //     //          Serial.println("writing eeprom username:");
      //     //          for (int i = 0; i < username.length(); ++i) {
      //     //            EEPROM.write(96 + i, username[i]);
      //     //            Serial.print("Wrote: ");
      //     //            Serial.println(username[i]);
      //     //          }
      //     //
      //     //          Serial.println("writing eeprom password:");
      //     //          for (int i = 0; i < password.length(); ++i) {
      //     //            EEPROM.write(128 + i, password[i]);
      //     //            Serial.print("Wrote: ");
      //     //            Serial.println(password[i]);
      //     //          }
      //     //
      //     //          Serial.println("writing eeprom uqid:");
      //     //          for (int i = 0; i < uqid.length(); ++i) {
      //     //            EEPROM.write(196 + i, uqid[i]);
      //     //            Serial.print("Wrote: ");
      //     //            Serial.println(uqid[i]);
      //     //          }

      //     // delay(100);
      //     // EEPROM.commit();
      //     // delay(100);
      //     // ESP.restart();
      //     // WiFi.begin(char_array1, char_array2);
      //   // }
      //   Serial.println("");
      // }
    }
  }

}
