
struct Config {
  char hostname[64] = "rgbclock";
  int min_brightness = 5;
  bool celsius = false;
  int display_offset = 0;
  float max_lux = 24;
  
  char ntpServer[64] = "0.pool.ntp.org";
  int gmtOffset = -8;
  bool clock_format = true;
  bool enable_authentication = false;

  bool debug = true;
};