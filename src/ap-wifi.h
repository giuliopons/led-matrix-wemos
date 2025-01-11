#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <main.h>

extern bool          WIFI_STATUS;          // global variable for wifi status
extern bool          AP_STATUS;            // global variable for access point mode status


// FS AND EEPROM LIBRARIES
// ----------------------------------------------------
#include <EEPROM.h>
#include <string.h>
#include <LittleFS.h>
extern bool    FSActive;

// Structure for data saved in EEPROM
struct settings {
  char ssid[31];    // /0 char at end
  char password[31];// /0 char at end
  char text[51];    // /0 char at end
  char effect[2];   // /0 char at end
  char brightness[4];   // /0 char at end
  char color[8];   // /0 char at end
  char check[3];    // [0]=>1 if wifi is configured [1]=>1 if settings is configured      /0 char at end
};
extern settings userdata;


//
// WEMOS FUNCTION, WIFI
// AP, PORTAL, EPROM...
// ----------------------------------------------------

extern DNSServer dnsServer;
extern ESP8266WebServer webServer;


String projectName();

// Define the type for the callback function
#include <functional>
// Usa std::function per maggiore flessibilità
typedef std::function<void()> CallbackFunction;

// SPIFFS AND EEPROM FUNCTIONS
// -----------------------------------------------------

// Read file from file system 
String readFile(String filename);

// Read user data from EEPROM (wifi pass...)
void readUserData();



// WIFI FUNCTIONS
// -----------------------------------------------------

void handleFile(String path, String keys[], String values[], size_t numReplacements);

// AP PORTAL: homepage, load home.html from SPIFFS
void handleHomeMenu();

// AP PORTAL: handle css
void handleCSS();

// AP PORTAL: form that shows SSID/password fields (also POST variables)
void handleFormWifiSetup();

// AP PORTAL: another form for more settings
// void handleFormSettings(CallbackFunction callback = NULL);
void handleFormSettings();


// Try wi-fi connection for "sec" seconds, return true on success
boolean tryWifi(int sec);

// Connect to wifi, tries multiple "times", if fail and startSetupPortal is true launch AP portal
void connectToWifi(int times = 3, bool startSetupPortal = false, CallbackFunction callback= NULL);

// AP PORTAL: handle stop AP url
void handleStopAP();

// AP PORTAL: setup the portal
void setupPortal(CallbackFunction callback= NULL);
