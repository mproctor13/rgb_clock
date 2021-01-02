
#include <NTPClient.h>
#include <time.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include "TimeUtil.h"

TimeUtil::TimeUtil(){
//  httpServer = httpServer;
}

void TimeUtil::setup(AsyncWebServer *httpServer){
    httpServer = httpServer;
    Serial.println("Start timeClient...");
    preferences.begin("rgb_clock", false);
    ntpServer = preferences.getString("ntpServer", "0.pool.ntp.org");
    gmtOffset = preferences.getLong("gmtOffset", -8);
    clock_format = preferences.getBool("clock_format", true);
    preferences.end();
    Serial.printf("NTP Settings: Server: %s, offest %i, Format: %i\n", ntpServer.c_str(), gmtOffset, clock_format);
    configTime(gmtOffset*3600, 0, ntpServer.c_str());
    httpServer->on("/clock", HTTP_GET, [&](AsyncWebServerRequest *request){
      char dateTime[24] = "12:12:12";
      
      sprintf(dateTime, "%i:%02d:%02d",
                        clock_hour(), clock_min(), clock_sec());
      request->send(200, "text/plain", dateTime);
    });
    Serial.println("End timeClienteClient...");
}

void TimeUtil::setClock_format(bool clock_format) {
  clock_format=clock_format;
  preferences.begin("rgb_clock", false);
  preferences.putBool("clock_format", clock_format);
  preferences.end();
}
void TimeUtil::setNTPServer(String ntpServer) {
  ntpServer=ntpServer;
  preferences.begin("rgb_clock", false);
  preferences.putString("ntpServer", ntpServer);
  preferences.end();
}
void TimeUtil::setGMTOffset(long gmtOffset) {
  gmtOffset=gmtOffset;
  preferences.begin("rgb_clock", false);
  preferences.putLong("gmtOffset", gmtOffset);
  preferences.end();
}

int TimeUtil::clock_hour() {
  time_t  t;
  struct tm *tm;
  int hour;

  t = time(NULL);
  tm = localtime(&t);
  if( clock_format ) {
    if( tm->tm_hour > 12 ) {
      hour = tm->tm_hour-12;
    }
    else if( tm->tm_hour == 0 ) {
      hour = 12;
    }
  }
  else{
    hour = tm->tm_hour;
  }
  
  return hour;
}

int TimeUtil::clock_min() {
  time_t  t;
  struct tm *tm;

  t = time(NULL);
  tm = localtime(&t);
  
  return tm->tm_min;
}

int TimeUtil::clock_sec() {
  time_t  t;
  struct tm *tm;

  t = time(NULL);
  tm = localtime(&t);
  
  return tm->tm_sec;
}
