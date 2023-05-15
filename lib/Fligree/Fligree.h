#ifndef fl
#define f1

#if (ARDUINO >100)
  #include "Arduino.h"
#else
  #include "Wrogram.h"
#endif

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <EEPROM.h>

class Fligree{
 
  public:
      
    Fligree(void);
    void begin(String url, String uname, String passwd, String uid);
    String  GETData(String endpoint);
    
    String POSTData(String endpoint, String D0, String data);

  private:
    String payload, code, url, D0, data, data_to_send, URI, URL, uqid, username, password;
    String httpRequestData;
    int httpCode;

};

#endif
