#include "ap-wifi.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "main.h"

bool          WIFI_STATUS = false;                 // global variable for wifi status
bool          AP_STATUS = false;            // global variable for access point mode status


// FS AND EEPROM LIBRARIES
// ----------------------------------------------------
#include <EEPROM.h>
#include <string.h>
#include <LittleFS.h>

// Structure for data saved in EEPROM
settings userdata = {};


extern void resetFlag(int i); // Dichiarazione esplicita della funzione



//
// WEMOS FUNCTION, WIFI
// AP, PORTAL, EPROM...
// ----------------------------------------------------

DNSServer dnsServer;
ESP8266WebServer webServer(80);

String projectName() {
    uint32_t chipId = ESP.getChipId();
    char projectname[50];
    sprintf(projectname, "ledmatrix_%X", chipId);
    return String(projectname);
}

// FS AND EEPROM FUNCTIONS
// -----------------------------------------------------

// Read user data from EEPROM (wifi pass...)
void readUserData(){
  EEPROM.begin(sizeof(struct settings) );
  EEPROM.get( 0, userdata );
}





// WIFI FUNCTIONS
// -----------------------------------------------------


void handleFile(String path, String keys[], String values[], size_t numReplacements) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.print("File not found: ");
        Serial.println(path);
        webServer.send(404, "text/plain", "404: File Not Found");
        return;
    }

    Serial.printf("Serving %s, size: %d bytes\n", path.c_str(), file.size());

    // Leggi tutto il contenuto del file
    String content = file.readString();
    file.close();

    for (size_t i = 0; i < numReplacements; i++) {
        content.replace(keys[i], values[i]);
    }

    // Determina il content type
    String contentType = "text/plain";
    if (path.endsWith(".css")) {
        contentType = "text/css";
    } else if (path.endsWith(".js")) {
        contentType = "application/javascript";
    } else if (path.endsWith(".html")) {
        contentType = "text/html";
    }

    webServer.setContentLength(content.length());
    webServer.sendHeader("Content-Type", contentType);
    webServer.sendHeader("Connection", "close");
    webServer.client().write(content.c_str(), content.length());
    webServer.client().stop(); // Chiude la connessione
}


// AP PORTAL: homepage, load home.html from FS
void handleHomeMenu() {
    handleFile("/home.html", {}, {}, 0);
}

// AP PORTAL: handle css
void handleCSS() {
  handleFile("/style.css", {}, {}, 0);
}

// AP PORTAL: form that shows SSID/password fields (also POST variables)
void handleFormWifiSetup() {

  if (webServer.method() == HTTP_POST) {

    strncpy(userdata.ssid,     webServer.arg("ssid").c_str(),     sizeof(userdata.ssid) );
    strncpy(userdata.password, webServer.arg("password").c_str(), sizeof(userdata.password) );
    userdata.ssid[webServer.arg("ssid").length()] = userdata.password[webServer.arg("password").length()] = '\0';
    userdata.check[0] = '1';
    userdata.check[1] = userdata.check[1] == '1' ? '1' : '0';
    userdata.check[2] = '\0';

    EEPROM.put(0, userdata);
    EEPROM.commit();
    Serial.println("saved");

    handleFile("/savedsettings.html", {}, {}, 0);
      
  } else {

    String keys[] = {"#ssid#", "#pwd#"};
    String s = userdata.ssid;
    String p = userdata.password;
    String values[] = {
        userdata.check[0] != '1' ? "" : s,
        userdata.check[0] != '1' ? "" : p
    };

    handleFile("/wifisetup.html", keys, values, 2);

  }
  
}

// AP PORTAL: another form for more settings (meteo url...) (TO DO)
void handleFormSettings() {


  if (webServer.method() == HTTP_POST) {

    strlcpy(userdata.text, webServer.arg("text").c_str(), sizeof(userdata.text));
    strlcpy(userdata.effect, webServer.arg("effect").c_str(), sizeof(userdata.effect));
    strlcpy(userdata.brightness, webServer.arg("brightness").c_str(), sizeof(userdata.brightness));
    strlcpy(userdata.color, webServer.arg("color").c_str(), sizeof(userdata.color));
    
    userdata.check[1] = '1'; // check control data

    userdata.check[0] = userdata.check[0] == '1' ? '1' : '0';
    userdata.check[1] = '1';
    userdata.check[2] = '\0';

    EEPROM.put(0, userdata);
    EEPROM.commit();

    handleFile("/savedsettings.html", {}, {}, 0);
    resetFlag( 5 );
    
  } else {
    String keys[] = {"#text#", "#effect#", "#brightness#", "#color#"};
    String values[] = {
        userdata.check[1] != '1' ? "" : userdata.text,
        userdata.check[1] != '1' ? "" : userdata.effect,
        userdata.check[1] != '1' ? "" : userdata.brightness,
        userdata.check[1] != '1' ? "" : userdata.color
    };
    handleFile("/settings.html", keys, values, 4);
    
  }
  
}


// Try wi-fi connection for "sec" seconds, return true on success
boolean tryWifi(int sec) {
    // comparison with string literal results in unspecified behavior
    if(userdata.check[0] != '1' || strcmp(userdata.ssid, "") == 0 ) {
        Serial.println("wifi not configured");
        return false;
    }

  WiFi.mode(WIFI_STA);
  Serial.println("Try wifi");
  Serial.println(userdata.ssid);
  Serial.println(userdata.password);
  WiFi.begin(userdata.ssid, userdata.password);

  byte tries = 0;
  byte onOffLed = 0;
  while (WiFi.status() != WL_CONNECTED) {
    onOffLed++;
    Serial.print(".");
    if(onOffLed % 2==1) digitalWrite(LED_BUILTIN, LOW); else digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    if (tries++ > sec) {
      // fail to connect
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("");
      return false;
    }
  }
  Serial.println("");
  digitalWrite(LED_BUILTIN, HIGH);
  return true; 
}

// Connect to wifi, tries multiple "times", if fail and startSetupPortal is true launch AP portal
void connectToWifi(int times, bool startSetupPortal, CallbackFunction callback) {
  int i =0;
  while(i<times && WIFI_STATUS==false) {
    WIFI_STATUS = tryWifi(10);
    i++;
  }
  if(!WIFI_STATUS) {
    if(startSetupPortal) setupPortal( callback );
    Serial.println("WIFI not found");
  } else {
    Serial.println("WIFI OK");
    
    if (MDNS.begin( projectName() )) {
      Serial.println("MDNS responder started");
    }

  }

}

// AP PORTAL: handle stop AP url
void handleStopAP(){
  
    String keys[] = {"#title#", "#msg#"};
    String values[] = {
        "Stop AP",
        "Access point stopped."
    };
    handleFile("/generic.html", keys,values, 2);
    WiFi.softAPdisconnect(true);
    AP_STATUS = false;
    connectToWifi();
}

// AP PORTAL: setup the portal
// callback is a function which is called repeatedly during the portal
void setupPortal(CallbackFunction callback) {
    
  Serial.println("");
  Serial.println("Start access point");
  AP_STATUS = true;

  // http://192.168.4.1
  IPAddress apIP(192, 168, 4, 1);

  Serial.println(projectName());
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP( projectName() );

  // replay to all requests with same HTML
  webServer.onNotFound([]() {

    webServer.send(200, "text/html", "<HTML><HEAD><meta http-equiv='refresh' content='3; url=/'></HEAD><BODY>Captive Portal<script>location.href='/';</script></BODY></HTML>");
    
      
  });
  webServer.on("/generate_204", []() {  // Android captive portal
    // webServer.sendHeader("Location", "/", true); // Reindirizza alla home del captive portal
    // webServer.send(302, "text/plain", ""); // Codice HTTP 302: Found

    webServer.send(204, "text/html", "<HTML><HEAD><meta http-equiv='refresh' content='3; url=/'></HEAD><BODY>Captive Portal<script>location.href='/';</script></BODY></HTML>");

  });
  webServer.on("/hotspot-detect.html", []() {  // Apple captive portal
      webServer.send(200, "text/html", "<HTML><HEAD><meta http-equiv='refresh' content='3; url=/'></HEAD><BODY>Captive Portal<script>location.href='/';</script></BODY></HTML>");
  });
  webServer.on("/ncsi.txt", []() {  // Windows captive portal
      webServer.sendHeader("Location", "/", true); // Reindirizza alla home del captive portal
      webServer.send(302, "text/plain", ""); // Codice HTTP 302: Found
  });

  webServer.on("/",  handleHomeMenu);           // HOME
  webServer.on("/style.css",  handleCSS);       // CSS
  webServer.on("/wifisetup",  handleFormWifiSetup);   // WIFI SETTINGS
  webServer.on("/settings", handleFormSettings ); // LED STRIP SETTINGS
  webServer.on("/stopap",  handleStopAP);             // STOP AP
  
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(53, "*", apIP);
  
  webServer.begin();
  Serial.println("WEB SERVER STARTED");

  // never ending loop waiting access point configuration     
  unsigned long timer = millis() + 70;
  while(AP_STATUS){
    delay(10);
    dnsServer.processNextRequest();
    webServer.handleClient();
    
    if(millis() > timer) {
      timer = millis() + 70;
      if( callback != NULL ) {
        
        callback();
      }
    }
    


  }
  
}
