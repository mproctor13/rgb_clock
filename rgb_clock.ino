#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <AsyncElegantOTA.h>

#define I2C_SDA 15
#define I2C_SCL 14
#include <Wire.h>
#include <SPI.h>

#include <MAX44009.h>

MAX44009 light;

#include "TimeUtil.h"
#include "BMESensor.h"
//#include "CamUtil.h"


#define APNAME "nsl_clock"
#define APPASSWORD "nsl_clock"
#define BUILTIN_LED 33
#define FLASH_LED 4
//#define HOSTIDENTIFY  "NSLClock"
//#define HOSTIDENTIFY  "RGBClock"
#define mDNSUpdate(c)  do {} while(0)
// Fix hostname for mDNS. It is a requirement for the lightweight update feature.
static const char* host = HOSTIDENTIFY;
#define HTTP_PORT 80
int LUX_NOT_FOUND = true;
bool SERIAL_DEBUG = true;
int min_brightness = 5;
float max_lux = 15;

#include "Display.h"
Display clock_display;

AsyncWebServer server(80);
TimeUtil timeutil = TimeUtil();
DNSServer dns;
float current_lux = 0;


void configModeCallback (AsyncWiFiManager *myWiFiManager) {
  if( SERIAL_DEBUG ){
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
  
    Serial.println(myWiFiManager->getConfigPortalSSID());
  }
}

int calculate_brightness(){
  int brightness = 5;

  if(current_lux > max_lux){
    brightness = 255;
  }
  else{
    brightness = min_brightness+int(float(current_lux/max_lux)*float(255.0-min_brightness));
  }
  return brightness;
}

void process_lux(){
  extern int loop_counter;
  int brightness = 5;
  
  if( !LUX_NOT_FOUND ){
    current_lux = light.get_lux();
    if( SERIAL_DEBUG ){
      Serial.print("Light (lux): ");
      Serial.print(current_lux);
      Serial.print(" setting brightness: ");
      brightness = calculate_brightness();
      clock_display.set_brightness(brightness);
      Serial.println(brightness);
    }
  }
  else if( loop_counter%5000 ){
    Serial.println(" Initializing MAX44009 sensor.");
    LUX_NOT_FOUND = light.begin();
    if( SERIAL_DEBUG && LUX_NOT_FOUND ){
      Serial.println("   Could not find a valid MAX44009 sensor, check wiring!");
    }
  }
}

    
void setup() {
  extern BMESensor *bme;
  bool result = false;
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  pinMode(FLASH_LED, OUTPUT);
  digitalWrite(FLASH_LED, LOW);
//  if(!initCamera()){
//    Serial.printf("Failed to initialize camera...");
//  }

  //first parameter is name of access point, second is the password
  AsyncWiFiManager wifiManager(&server,&dns);
  wifiManager.setAPCallback(configModeCallback);
  
//  wifiManager.resetSettings(); //reset settings - for testing
//  result = wifiManager.startConfigPortal(APNAME, APPASSWORD);
  result = wifiManager.autoConnect(APNAME, APPASSWORD);
  if( result ) {
    if (MDNS.begin(host)) {
      MDNS.addService("http", "tcp", HTTP_PORT);
      Serial.printf("RGB Clock ready! Open http://%s.local/ in your browser\n", host);
    }
    else
      Serial.println("Error setting up MDNS responder");
      
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", "Hello World");
    });
 
    server.on("/rand", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", String(random(1000)));
    });
 
    server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
      char state[500];

      sprintf(state, "{'time': '%i:%02d:%02d', 'temperature': '%i', 'humidity': '%f', 'pressure': '%f', 'lux': '%f', 'brightness': '%i', 'max_lux': '%f', 'min_brightness': '%i'}",
                        timeutil.clock_hour(), timeutil.clock_min(), timeutil.clock_sec(),
                        bme->get_tempatureF(), 
                        bme->get_humidity() / 1000.0F,
                        bme->get_pressure() / 3386.0F,
                        current_lux,
                        calculate_brightness(), max_lux, min_brightness
                        );
      request->send(200, "application/json", String(state));
    });
//    server.on("/bmp", HTTP_GET, sendBMP);
//    server.on("/capture", HTTP_GET, sendJpg);
//    server.on("/stream", HTTP_GET, streamJpg);
//    server.on("/control", HTTP_GET, setCameraVar);
//    server.on("/status", HTTP_GET, getCameraStatus);
   
    Wire.begin(I2C_SDA, I2C_SCL); // Init Wire for i2c
    bme = new BMESensor();
    process_lux();
    timeutil.setup(&server);
    clock_display.setup(4);
    AsyncElegantOTA.begin(&server);    // Start ElegantOTA
    server.begin();
    Serial.println("HTTP server started");

    if(!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      bool formatted = SPIFFS.format();
      if ( formatted ) {
        Serial.println("SPIFFS formatted successfully");
      } else {
        Serial.println("Error formatting");
      }
    }
    
    Serial.print("totalBytes: ");
    Serial.print(SPIFFS.totalBytes());
    Serial.print(" usedBytes: ");
    Serial.println(SPIFFS.usedBytes());
  }
}
int loop_counter=0;
BMESensor *bme;
void loop() {
  AsyncElegantOTA.loop();
  if(loop_counter % 5000 == 0){
    bme->process();
  }
  if(loop_counter % 1000 == 0){
    process_lux();
    // ************* Blink LED *******************
    if(loop_counter%2000 == 0){
      digitalWrite(BUILTIN_LED, 0);
    }
    else{
      digitalWrite(BUILTIN_LED, 1);
    }
  }
  if(loop_counter % 20 == 0){
    clock_display.handle_display(loop_counter);
  }
  delay(1);
  loop_counter++;
}
