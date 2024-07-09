#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <AsyncElegantOTA.h>
#include <ESPUI.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Ticker.h> //for LED status

#define I2C_SDA 15
#define I2C_SCL 14
#include <Wire.h>
#include <SPI.h>

#include <MAX44009.h>

MAX44009 light;

#include "TimeUtil.h"
#include "Config.h"
#include "BMESensor.h"
#include "CamUtil.h"


#define APNAME "rgb_clock"
#define APPASSWORD "rgb_clock"
#define BUILTIN_LED 33 // esp32cam
// #define BUILTIN_LED 25 // htit-wb32
#define FLASH_LED 4
#define HOSTIDENTIFY  "NSLClock"
// #define mDNSUpdate(c)  do {} while(0)
#define mDNSUpdate(c)  do { c.update(); } while(0)
// Fix hostname for mDNS. It is a requirement for the lightweight update feature.
static const char* host = HOSTIDENTIFY;
#define HTTP_PORT 80
bool USE_HARDWARE = true;
int LUX_NOT_FOUND = true;
bool shouldSaveConfig = false;
Ticker ticker;

#include "Display.h"
Display clock_display;

AsyncWebServer *setupServer = new AsyncWebServer(80);
TimeUtil timeutil = TimeUtil();
AsyncWiFiManager *wifiManager;
Config config;
TaskHandle_t CameraTask;
DNSServer dns;
float current_lux = 0;
bool manual_mode = false;
String manual_color = "#FF0000";
uint16_t clock_ui, ntp_ui, unit_ui, format_ui, min_ui, display_ui, lux_ui, zone_ui, auth_ui, saveCal, all_on, all_off, text_color;

void tick() {
  //toggle state
  int state = digitalRead(BUILTIN_LED); 
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

void configModeCallback (AsyncWiFiManager *myWiFiManager) {
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
  if( config.debug ){
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
  
    Serial.println(myWiFiManager->getConfigPortalSSID());
  }
}

void saveConfigCallback () {
  if( config.debug )
    Serial.println("Should save config");
  shouldSaveConfig = true;
}


void settingsCallback(Control* sender, int type){
  if( config.debug ){
    Serial.print("calCallback called by ");
    Serial.println(sender->id);
  }

// ntp_ui, format_ui, min_ui, display_ui, lux_ui, zone_ui, saveCal
  if( sender->id == ntp_ui  ){
    if( type == T_VALUE ){
      strlcpy(config.ntpServer, sender->value.c_str(), sizeof(config.ntpServer));
    }
  }
  else if( sender->id == zone_ui ){
    if(type == N_VALUE) {
      int offset = atoi(sender->value.c_str());
      if( offset > -12 && offset < 12 )
        config.gmtOffset = offset;
      else
        ESPUI.updateNumber(zone_ui, config.gmtOffset);
    }
  }
  else if( sender->id == format_ui ){
    config.clock_format = sender->value.toInt();
  }
  else if( sender->id == min_ui ){
    if(type == N_VALUE) {
      config.min_brightness = atoi(sender->value.c_str());
    }
  }
  else if( sender->id == lux_ui ){
    if(type == N_VALUE) {
      config.max_lux = atoi(sender->value.c_str());
    }
  }
  else if( sender->id == display_ui ){
    if(type == N_VALUE) {
      config.display_offset = atoi(sender->value.c_str());
    }
  }
  else if( sender->id == unit_ui ){
    config.celsius = sender->value.toInt();
  }
  else if( sender->id == auth_ui ){
    config.enable_authentication = sender->value.toInt();
  }
  else if( sender->id == saveCal ){
    if(type == B_UP) {
      saveConfig();
    }
  }

}
void restart(Control* sender, int type){
  if(type == B_UP) {
    if( config.debug ){
      Serial.print("reset network callback called by ");
      Serial.println(sender->id);
    }
    ESP.restart();
  }
}

void resetNetwork(Control* sender, int type){
  if(type == B_UP) {
    if( config.debug ){
      Serial.print("reset network callback called by ");
      Serial.println(sender->id);
    }
    wifiManager->resetSettings();
    ESP.restart();
  }
}

void manualMode(Control* sender, int type){
  if( config.debug ){
    Serial.print("Manual Mode called with value: ");
    Serial.println(sender->value.toInt());
  }
  manual_mode = sender->value.toInt();
  ESPUI.setEnabled(all_on, manual_mode);
  ESPUI.setEnabled(all_off, manual_mode);
  ESPUI.setEnabled(text_color, manual_mode);
}

void allControl(Control* sender, int type){
  if(type == B_UP) {
    if( config.debug ){
      Serial.print("All On called by ");
      Serial.println(sender->id);
    }
    if(sender->id == all_on){
      long temp = strtol(&(manual_color.c_str())[1], NULL, 16);
      if( config.debug ){
        Serial.print("long value: ");
        Serial.println(temp);
      }
      int r = temp >> 16;
      int g = temp >> 8 & 0xFF;
      int b = temp & 0xFF;
      clock_display.allOn(r, g, b);
      clock_display.show();
      if( config.debug )
        Serial.println("All On Shown!");
    }

    if(sender->id == all_off){
      clock_display.allOff();
      clock_display.show();
      if( config.debug )
        Serial.println("All Off Shown!");
    }
  }
}
void colorCallback(Control* sender, int type){
  if( config.debug ){
    Serial.print("color value: ");
    Serial.println(sender->value);
  }
  manual_color = sender->value;
  long temp = strtol(&(sender->value.c_str())[1], NULL, 16);
  if( config.debug ){
    Serial.print("long value: ");
    Serial.println(temp);
  }
  // int r = temp >> 16;
  // int g = temp >> 8 & 0xFF;
  // int b = temp & 0xFF;
  // DEBUG
  // Serial.print("RGB: ");
  // Serial.print(r, DEC);
  // Serial.print(" ");
  // Serial.print(g, DEC);
  // Serial.print(" ");
  // Serial.print(b, DEC);
  // Serial.println(" ");

}

void loadConfig(){
  Serial.println("Load config");
  if (SPIFFS.exists("/config.json")) {
    //file exists, reading and loading
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
      if( config.debug )
        Serial.println("opened config file");
      StaticJsonDocument<512> configDoc;
      if( deserializeJson(configDoc, configFile) )
        Serial.println(F("Failed to read file, using default configuration"));

      strlcpy(config.hostname, configDoc["hostname"] | "rgbclock", sizeof(config.hostname));

      config.min_brightness = configDoc["min_brightness"];
      config.celsius = configDoc["celsius"];
      config.display_offset = configDoc["display_offset"];
      config.max_lux = configDoc["max_lux"];

      strlcpy(config.ntpServer, configDoc["ntpServer"] | "0.pool.ntp.org", sizeof(config.ntpServer));
      config.gmtOffset = configDoc["gmtOffset"];
      config.clock_format = configDoc["clock_format"];
      config.enable_authentication = configDoc["enable_authentication"];

      config.debug = configDoc["debug"];
      configFile.close();
    }
  }
  else{ // Go ahead and write config
    shouldSaveConfig = true;
  }
}

void saveConfig(){
  Serial.println("saving config");
  File configFile = SPIFFS.open("/config.json", "w");
  if(configFile){
    StaticJsonDocument<256> json;
    json["hostname"] = config.hostname;
    json["min_brightness"] = config.min_brightness;
    json["celsius"] = config.celsius;
    json["display_offset"] = config.display_offset;
    json["max_lux"] = config.max_lux;
    json["ntpServer"] = config.ntpServer;
    json["gmtOffset"] = config.gmtOffset;
    json["clock_format"] = config.clock_format;
    json["enable_authentication"] = config.enable_authentication;

    json["debug"] = config.debug;
    if(serializeJson(json, configFile) == 0){
      Serial.println(F("Failed to write to file"));
    }
    configFile.close();
  }
  else{
    Serial.println("failed to open config file for writing!");
  }
}

int calculate_brightness(){
  int brightness = 5;

  if(current_lux > config.max_lux){
    brightness = 255;
  }
  else{
    brightness = config.min_brightness+int(float(current_lux/config.max_lux)*float(255.0-config.min_brightness));
  }
  return brightness;
}

void process_lux(){
  extern int loop_counter;
  extern bool USE_HARDWARE;
  int brightness = 5;
  
  if( !USE_HARDWARE ){
    current_lux = random(0, config.max_lux);
    brightness = calculate_brightness();
    clock_display.set_brightness(brightness);
  }
  else if( !LUX_NOT_FOUND ){
    current_lux = light.get_lux();
    if( config.debug ){
      Serial.print("Light (lux): ");
      Serial.print(current_lux);
      Serial.print(" setting brightness: ");
      brightness = calculate_brightness();
      clock_display.set_brightness(brightness);
      Serial.println(brightness);
    }
  }
  else if( loop_counter%5000 ){
    if( config.debug )
      Serial.println(" Initializing MAX44009 sensor.");
    LUX_NOT_FOUND = light.begin();
    if( LUX_NOT_FOUND ){
      Serial.println("   Could not find a valid MAX44009 sensor, check wiring!");
    }
  }
}

    
void setup() {
  extern BMESensor *bme;
  bool result = false;
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Preferences preferences;
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  // start ticker with 0.6 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  if(!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
     bool formatted = SPIFFS.format();
    if ( formatted ) {
      Serial.println("SPIFFS formatted successfully");
    }
    else {
      Serial.println("Error formatting");
    }
  }
  if( config.debug ){
    Serial.print("totalBytes: ");
    Serial.print(SPIFFS.totalBytes());
    Serial.print(" usedBytes: ");
    Serial.println(SPIFFS.usedBytes());
  }
  loadConfig();
  
  if( config.debug )
    Serial.printf("Default hostname: %s\n", WiFi.getHostname());
  
  //Set new hostname
  WiFi.hostname(config.hostname);

  if( config.debug ) //Get Current Hostname
    Serial.printf("New hostname: %s\n", config.hostname);

  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  pinMode(FLASH_LED, OUTPUT);
  digitalWrite(FLASH_LED, LOW);

  wifiManager = new AsyncWiFiManager(setupServer,&dns);
  wifiManager->setAPCallback(configModeCallback);
  AsyncWiFiManagerParameter custom_hostname("hostname", "Hostname", config.hostname, 40);
  wifiManager->setSaveConfigCallback(saveConfigCallback);
  wifiManager->addParameter(&custom_hostname);
  
  result = wifiManager->autoConnect(APNAME, APPASSWORD);
  if( result ) {
    delete setupServer;
    strlcpy(config.hostname, custom_hostname.getValue(), sizeof(config.hostname));
    if (shouldSaveConfig) {
      saveConfig();
    }
    if (MDNS.begin(config.hostname)) {
      MDNS.addService("http", "tcp", HTTP_PORT);
      Serial.printf("RGB Clock ready! Open http://%s.local/ in your browser\n", host);
    }
    else
      Serial.println("Error setting up MDNS responder");
    
    //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
    // xTaskCreatePinnedToCore(
    //                 cameraTask,   /* Task function. */
    //                 "CameraTask",     /* name of task. */
    //                 10000,       /* Stack size of task */
    //                 NULL,        /* parameter of the task */
    //                 1,           /* priority of the task */
    //                 &CameraTask,      /* Task handle to keep track of created task */
    //                 0);          /* pin task to core 0 */ 



    uint16_t clock_tab = ESPUI.addControl(ControlType::Tab, "Time", "Time");
    clock_ui = ESPUI.addControl(ControlType::Label, "Time", "vv", ControlColor::Dark, clock_tab);

    uint16_t manual_tab = ESPUI.addControl(ControlType::Tab, "Manual", "Manual");
    
    uint16_t manual_switch = ESPUI.addControl(ControlType::Switcher, "Manual Mode", String(manual_mode), ControlColor::Dark, manual_tab, &manualMode);
    text_color = ESPUI.addControl(ControlType::Text, "Color", manual_color, ControlColor::Dark, manual_tab, &colorCallback);
    // text_color = ESPUI.text("Color", &colorCallback, ControlColor::Dark, "#FF0000");
    ESPUI.setInputType(text_color, "color");
    ESPUI.setEnabled(text_color, manual_mode);

    all_on  = ESPUI.addControl(ControlType::Button, "Control", "All On", ControlColor::Dark, manual_tab, &allControl);
    all_off = ESPUI.addControl(ControlType::Button, "", "All Off", ControlColor::Dark, all_on, &allControl);
    ESPUI.setEnabled(all_on, manual_mode);
    ESPUI.setEnabled(all_off, manual_mode);

    uint16_t setup_tab = ESPUI.addControl(ControlType::Tab, "Settings", "Settings");
    
    min_ui = ESPUI.addControl(ControlType::Number, "Minimum Brightness", String(config.min_brightness), ControlColor::Dark, setup_tab, &settingsCallback);
    lux_ui = ESPUI.addControl(ControlType::Number, "Max LUX", String(config.max_lux), ControlColor::Dark, setup_tab, &settingsCallback);
    display_ui = ESPUI.addControl(ControlType::Number, "Tempature Offset", String(config.display_offset), ControlColor::Dark, setup_tab, &settingsCallback);
    
    ntp_ui = ESPUI.addControl(ControlType::Text, "NTP Server", config.ntpServer, ControlColor::Dark, setup_tab, &settingsCallback);
    ESPUI.addControl(ControlType::Max, "", "64", ControlColor::None, ntp_ui);
    zone_ui = ESPUI.addControl(ControlType::Number, "Timezone Offset(hours)", String(config.gmtOffset), ControlColor::Dark, setup_tab, &settingsCallback);
	  ESPUI.addControl(Min, "", "-12", None, zone_ui);
	  ESPUI.addControl(Max, "", "12", None, zone_ui);
    format_ui = ESPUI.addControl(ControlType::Switcher, "12 Hour Clock", String(config.clock_format), ControlColor::Dark, setup_tab, &settingsCallback);
    unit_ui = ESPUI.addControl(ControlType::Switcher, "Show Temp In Celsius", String(config.celsius), ControlColor::Dark, setup_tab, &settingsCallback);
    auth_ui = ESPUI.addControl(ControlType::Switcher, "Enable Authentication", String(config.enable_authentication), ControlColor::Dark, setup_tab, &settingsCallback);


    saveCal = ESPUI.addControl(ControlType::Button, "Update Settings", "Save", ControlColor::Dark, setup_tab, &settingsCallback);

    uint16_t reset = ESPUI.addControl(ControlType::Button, "System", "Reset Network", ControlColor::Dark, setup_tab, &resetNetwork);
    ESPUI.addControl(ControlType::Button, "", "Restart", ControlColor::Dark, reset, &restart);

    uint16_t ota_tab = ESPUI.addControl(ControlType::Tab, "Update", "Update");
    ESPUI.addControl(ControlType::Label, "<a href='/update'>OTA</a>", "<iframe width=\"450px\" height=\"750px\" src=\"/update\">", ControlColor::Dark, ota_tab);

    if( config.enable_authentication )
      ESPUI.begin("RGB Clock", APNAME, APPASSWORD);
    else
      ESPUI.begin("RGB Clock");
    AsyncElegantOTA.begin(ESPUI.server);    // Start ElegantOTA
    Serial.println("HTTP server started");
 
    ESPUI.server->on("/rand", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", String(random(8)));
    });
 
    ESPUI.server->on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
      char state[500];

      sprintf(state, "{\"time\": \"%i:%02d:%02d\", \"temperature\": \"%i\", \"temperatureC\": \"%i\", \"humidity\": \"%f\", \"pressure\": \"%f\", \"lux\": \"%f\", \"brightness\": \"%i\", \"max_lux\": \"%f\", \"min_brightness\": \"%i\", \"closed\": \"%s\", \"gmtoffset\": \"%i\"}",
                        timeutil.clock_hour(), timeutil.clock_min(), timeutil.clock_sec(),
                        bme->get_tempatureF(), 
                        bme->get_tempatureC(), 
                        bme->get_humidity() / 1000.0F,
                        bme->get_pressure() / 3386.0F,
                        current_lux,
                        calculate_brightness(), config.max_lux, config.min_brightness, 
                        clock_display.get_bracket() ? "true" : "false", config.gmtOffset
                        );
      request->send(200, "application/json", String(state));
    });
 
    ESPUI.server->on("/open", HTTP_GET, [](AsyncWebServerRequest *request){
      clock_display.set_bracket(false);
      request->send(200, "text/plain", "Open!");
    });
 
    ESPUI.server->on("/closed", HTTP_GET, [](AsyncWebServerRequest *request){
      clock_display.set_bracket(true);
      request->send(200, "text/plain", "Closed!");
    });
 
    ESPUI.server->on("/color", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", clock_display.active_color_html());
    });
    ESPUI.server->on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/config.json", "application/json");
    });
    
    Wire.begin(I2C_SDA, I2C_SCL); // Init Wire for i2c
    bme = new BMESensor();
    clock_display.setup(4);
    process_lux();
    timeutil.setup(ESPUI.server);
    Serial.print("Setup() running on core ");
    Serial.println(xPortGetCoreID());

    ticker.detach();
  }
}
int loop_counter=0;
BMESensor *bme;
void loop() {
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
  if(loop_counter % 20 == 0 ){
    if( !manual_mode )
      clock_display.handle_display(loop_counter);

  }
  delay(1);
  loop_counter++;
}
