#include <WiFi.h>
#include <Preferences.h>
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
#define HOSTIDENTIFY  "NSLClock"
#define mDNSUpdate(c)  do {} while(0)
// Fix hostname for mDNS. It is a requirement for the lightweight update feature.
static const char* host = HOSTIDENTIFY;
#define HTTP_PORT 80
int LUX_NOT_FOUND = true;
bool SERIAL_DEBUG = true;
int min_brightness = 5;
float max_lux = 15;
int display_offset = -5;

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
  Preferences preferences;
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
      request->send(200, "text/plain", String(random(8)));
    });
 
    server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
      char state[500];

      sprintf(state, "{'time': '%i:%02d:%02d', 'temperature': '%i', 'temperatureC': '%i', 'humidity': '%f', 'pressure': '%f', 'lux': '%f', 'brightness': '%i', 'max_lux': '%f', 'min_brightness': '%i'', 'closed': '%s'}",
                        timeutil.clock_hour(), timeutil.clock_min(), timeutil.clock_sec(),
                        bme->get_tempatureF(), 
                        bme->get_tempatureC(), 
                        bme->get_humidity() / 1000.0F,
                        bme->get_pressure() / 3386.0F,
                        current_lux,
                        calculate_brightness(), max_lux, min_brightness, 
                        clock_display.get_bracket() ? "true" : "false"
                        );
      request->send(200, "application/json", String(state));
    });
 
    server.on("/open", HTTP_GET, [](AsyncWebServerRequest *request){
      clock_display.set_bracket(false);
      request->send(200, "text/plain", "Open!");
    });
 
    server.on("/closed", HTTP_GET, [](AsyncWebServerRequest *request){
      clock_display.set_bracket(true);
      request->send(200, "text/plain", "Closed!");
    });
 
    server.on("/color", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", clock_display.active_color_html());
    });
    
    server.on("/set", HTTP_GET, setVars);
//    server.on("/bmp", HTTP_GET, sendBMP);
//    server.on("/bmp", HTTP_GET, sendBMP);
//    server.on("/capture", HTTP_GET, sendJpg);
//    server.on("/stream", HTTP_GET, streamJpg);
//    server.on("/control", HTTP_GET, setCameraVar);
//    server.on("/status", HTTP_GET, getCameraStatus);
   
    preferences.begin("rgb_clock", false);
    min_brightness = int(preferences.getLong("min_brightness", 5));
    max_lux = preferences.getFloat("max_lux", 15);
    preferences.end();
    
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

void setVars(AsyncWebServerRequest *request){
    if(!request->hasArg("var") || !request->hasArg("val")){
        request->send(404);
        return;
    }
    String var = request->arg("var");
    const char * variable = var.c_str();
    Serial.print("[");
    Serial.print(var);
    Serial.print("] = [");
    Serial.print(request->arg("val") );
    Serial.println("]");
    
//    int val = atoi(request->arg("val").c_str());
//    int res = 0;
//    if(!strcmp(variable, "framesize")) res = s->set_framesize(s, (framesize_t)val);
//    else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
//    else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
//
//    else {
//        log_e("unknown setting %s", var.c_str());
//        request->send(404);
//        return;
//    }
//    log_d("Got setting %s with value %d. Res: %d", var.c_str(), val, res);

    AsyncWebServerResponse * response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}
