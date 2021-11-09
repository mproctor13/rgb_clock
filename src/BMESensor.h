

#include "Zanshin_BME680.h"  // Include the BME680 Sensor library

class BMESensor {
 public:
  BMESensor();
  bool have_data();
  int get_tempature();
  int get_tempatureF();
  int get_tempatureC();
  int get_humidity();
  int get_pressure();
  void process();
 
 private:
  BME680_Class *sensor;
  int BME_FOUND;
  int32_t  tempature, humidity, pressure, gas;  // BME readings
  int reads;
};
