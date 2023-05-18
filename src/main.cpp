#include <Arduino.h>
#include <ArduinoJson.h>
#include <MFRC522.h> //MFRC522 library
#include "BluetoothSerial.h"
#include <Wire.h>
#include <base64.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>

#define SDA_SS_PIN 5 // 21 //ESP Interface Pin
#define RST_PIN 15   // 22    //ESP Interface Pin
#define ID_EEPROM_ADDRESS 226

MFRC522 mfrc522(SDA_SS_PIN, RST_PIN); // create instance of class
MFRC522::MIFARE_Key key;


int sw = 14;
const int LED1 = 1;
// Variables

String  ssid_pass;     // character and character string buffer
String uqid = "";
String username = "";
String password = "";

static int id;

// Function Decalration
bool checkWifi_connection(String username, String password, String uqid);
void send_data_to_bt_and_setup_sta(void);

BluetoothSerial btSerial;

void get_data()
{
  
  btSerial.end();

  StaticJsonDocument<1024> jsonDocument;
  DeserializationError error;  

  String input_s = username + ":" + password;
  String encoded = base64::encode(input_s);
  String auth = "Basic " + encoded;



  WiFiClientSecure client;
  client.setInsecure();

  Serial.println("[HTTP] begin...\n");
  if (client.connect("fleetkaptan.up.railway.app", 443))
  {
    client.println("GET /api/rfid/" + uqid + "/" + username + "/write-to-rfid/ HTTP/1.1");
    client.println("Host: fleetkaptan.up.railway.app");
    client.println("User-Agent: ESP32");
    client.println("Authorization: "+ auth);
    client.println("Connection: close");
    client.println();
    Serial.println(F("Data were sent successfully"));

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
          Serial.println("headers received");
          break;
        }
    }

    String response;
    if (client.available()) {
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


    int id = jsonDocument["id"];
    int storedId;

    storedId = int(EEPROM.read(ID_EEPROM_ADDRESS));

    if (id != storedId)
    {
      Serial.println("changes");
      EEPROM.write(ID_EEPROM_ADDRESS, id);
      EEPROM.commit();
      //write to rfid
    }
    else{

      Serial.println("Wont change");
    }
    const char* unique_id_recived_from_server = jsonDocument["esp"]["unique_id"];
    const char* value_recived_from_server = jsonDocument["value"];

    Serial.print("ID: ");
    Serial.println(id);
    Serial.print("Unique ID: ");
    Serial.println(unique_id_recived_from_server);
    Serial.print("Value: ");
    Serial.println(value_recived_from_server);
  }

  else
  {
    Serial.println(F("Connection wasnt established"));
  }
  Serial.println("we got the responnse");
  
  btSerial.begin(9600);
}


void post_data(String D0, String data){
  btSerial.end();
  delay(100);
   if (WiFi.status() == WL_CONNECTED)
  {
    StaticJsonDocument<256> jsonDocument;
    WiFiClientSecure client;

    client.setInsecure();

    Serial.println("[HTTP] begin...\n");
    // configure traged server and url
    // http.begin("https://www.howsmyssl.com/a/check", ca); //HTTPS
    String data = "D0=" + String(D0) + "&data=" + data + "&pass=parth";

    if (client.connect("fleetkaptan.up.railway.app", 443))
    {
      client.println("POST /api/rfid/" + uqid + "/" + username + "/read-esp-scanned HTTP/1.1");
      // client.println("POST /api/rfid/8yaotc0wilsu/parth/read-esp-scanned HTTP/1.1");
      client.println("Host: fleetkaptan.up.railway.app");
      client.println("User-Agent: ESP32");

      String input_s = username + ":" + password;
      char *input = new char[input_s.length() + 1];
      strcpy(input, input_s.c_str());
      String inputString = String(input);
      String encoded = base64::encode(inputString);
      String auth = "Basic " + encoded;

      // client.println("Authorization: Basic parth:123");
      client.println("Authorization: Basic " + auth);
      client.println("Content-Type: application/x-www-form-urlencoded;");
      client.println("Content-Length: " + String(data.length()));
      client.println();
      client.println(data);
      Serial.println(F("Data were sent successfully"));
      while (client.available() == 0)
        ;
      String c{};
      while (client.available())
      {
        c += (char)client.read();
      }

      Serial.println(c);
      DeserializationError error = deserializeJson(jsonDocument, c);

      if (error)
      {
        Serial.print("Deserialization error: ");
        Serial.println(error.c_str());
        return;
      }

      // Access the JSON data
      if (jsonDocument.containsKey("id") && !jsonDocument["id"].isNull()) {
    // Access the "id" value
        int id = jsonDocument["id"];
        const char *uniqueId = jsonDocument["esp"]["unique_id"];
        // const char *timestamp = jsonDocument["timestamp"];
        const char *value = jsonDocument["value"];
        Serial.print("ID: ");
        Serial.println(id);
        Serial.print("Unique ID: ");
        Serial.println(uniqueId);
        // Serial.print("Timestamp: ");
        // Serial.println(timestamp);
        Serial.print("Value: ");
        Serial.println(value);
      } else {
        Serial.println("ID not found or null");
      }

      delete[] input;
    }

    else
    {
      Serial.println(F("Connection wasnt established"));
    }
    Serial.println("we got the responnse");
    client.stop();
  }
  btSerial.begin(9600);
}

void setup()
{

  Serial.begin(9600);
  Serial.println("hey");
  delay(100);
  btSerial.begin(9600);
  delay(10);

  Serial.println("Ready");

  pinMode(sw, INPUT_PULLUP);


  EEPROM.begin(512); // Initialasing EEPROM
  delay(200);

  Serial.println("Reading EEPROM ssid");

  String esid;
  for (int i = 0; i < 32; ++i)
  {
    char c = char(EEPROM.read(i));
    if (c != 0) {
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
    if (c != 0) {
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
    if (c != 0) {
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
    if (c != 0) {
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
    if (c != 0) {
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

    SPI.begin(); // Initiate  SPI bus
    mfrc522.PCD_Init();
    return;
  }
  else
  {

    // Setup Bluetooth
    Serial.print("State of switch");
    Serial.print(digitalRead(sw));
    if (digitalRead(sw) == LOW)
    {
      Serial.println("Turning the Bluetooth On");
      send_data_to_bt_and_setup_sta();
    }
    else
    {
      Serial.println("Do nothing");
    }
  }

  Serial.println();
  Serial.println("Waiting...");
  
  
}

long last_time_scaned_rfid;
const char *AuthenticatedTag = "E3E7719B";

void loop()
{
  get_data();
  
  Serial.println("heya");
  
  if (digitalRead(sw) == LOW)
  {
    Serial.println("working");
    while (btSerial.available())
    {
      ssid_pass = btSerial.readString();
      String ssid_pass_2 = ssid_pass;
      Serial.println(ssid_pass);
      char *token;
    }
  }
  delay(10000);

}

bool checkWifi_connection(String username, String password, String uqid)
{
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while (c < 20)
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

  while (digitalRead(sw) == LOW)
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


