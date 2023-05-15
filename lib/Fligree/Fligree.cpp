#include "Fligree.h"
#include <string>
#include <cctype>
#include <ArduinoJson.h>

Fligree::Fligree(void){
  Serial.println("Constructor Called");
}

void Fligree::begin(String url, String uname, String passwd, String uid){
  URL = String(url.c_str());
  username = String(uname.c_str());
  password = String(passwd.c_str());
  
  uqid = String(uid);
  uqid.trim();  
  uqid.replace(" ", "");
  
  
  Serial.println("UQID in class:");
  Serial.println(uqid);

}


String Fligree::GETData(String endpoint){

  WiFiClient client;

  HTTPClient http;

  URI = URL+endpoint+"?uqid="+uqid+"&username="+username;
 
  http.begin(client, URI);

  Serial.println("GET, "+URI);

  httpCode = http.GET();
  if (httpCode > 0) {


    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      code = "200";
      Serial.print("GET DATA: ");
      http.end();
    }
  } else {
    Serial.print("[HTTP] GET... failed, error: "+String(http.errorToString(httpCode))+"\n,to URI: %s"+ URI);
    code = "400";
    http.end();
  } 
  Serial.println("");
  return payload;
};



String Fligree::POSTData(String endpoint, String D0, String data){

  
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClientSecure client;
    // HTTPClient http;
    client.setInsecure();

    Serial.println("[HTTP] begin...\n");
    // configure traged server and url
    // http.begin("https://www.howsmyssl.com/a/check", ca); //HTTPS
    String data = "D0=" + String(D0) + "&data=" + data;

    if (client.connect("fleetkaptan.up.railway.app", 443))
    {
      client.println("POST /api/rfid/"+uqid+"/"+username+"/read-esp-scanned HTTP/1.1");
      client.println("Host: fleetkaptan.up.railway.app");
      client.println("User-Agent: ESP32");
      client.println("Authorization: Basic parth:123");
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
      // JSONVar myObject = JSON.parse(c);
      // if (JSON.typeof(myObject) == "undefined")
      // {
      //   Serial.println("Parsing input failed!");
      // }

      // debug("JSON object = ");
      // Serial.println(myObject);

      // myObject.keys() can be used to get an array of all the keys in the object
      // JSONVar keys = myObject.keys();

      // for (int i = 0; i < keys.length(); i++)
      // {
      //   JSONVar value = myObject[keys[i]];
      //   debug(keys[i]);
      //   debug(" = ");
      //   Serial.println(value);
      // }
    }

    else
    {
      Serial.println(F("Connection wasnt established"));
    }
    Serial.println("we got the responnse");
    client.stop();
  }
};
