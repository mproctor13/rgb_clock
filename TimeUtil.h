
#include <Preferences.h>
#include <ESPAsyncWebServer.h>

class TimeUtil {
 public:
  TimeUtil();
  ~TimeUtil() {}
  void setup(AsyncWebServer *);
  int clock_hour();
  int clock_min();
  int clock_sec();
  
  bool getClock_format(){return clock_format;};
  String getNTPServer(){return ntpServer;};
  long getGMTOffset(){return gmtOffset;};
  
  void setClock_format(bool clock_format);
  void setNTPServer(String ntpServer);
  void setGMTOffset(long gmtOffset);


 private:
  AsyncWebServer *httpServer;
  String ntpServer;
  long gmtOffset;
  bool clock_format;
  Preferences preferences;
  
};
