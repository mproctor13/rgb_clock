
#include "BMESensor.h"
#include "Config.h"

BMESensor::BMESensor(){
  extern bool USE_HARDWARE;
  if( USE_HARDWARE ){
    sensor = new BME680_Class();
    BME_FOUND = false;
  }
  else
    BME_FOUND = true;
  reads = 0;
}
bool BMESensor::have_data(){
  return BME_FOUND;
}
int BMESensor::get_tempature(){
  return tempature;
}
int BMESensor::get_tempatureF(){
  extern Config config;
  return (1.8 * (tempature/100) + 32)+config.display_offset;
}
int BMESensor::get_tempatureC(){
  return tempature/100;
}
int BMESensor::get_humidity(){
  return humidity / 1000;
}
int BMESensor::get_pressure(){
  return pressure;
}
void BMESensor::process(){
  extern bool USE_HARDWARE;
  extern Config config;
  
  if( BME_FOUND ){
    if( USE_HARDWARE )
      sensor->getSensorData(tempature, humidity, pressure, gas, false);  // Get readings
    else{ // fake readings
      tempature = random(1,200);
      humidity = random(999,100000);
      pressure = random(3385,338600);
      Serial.println("Faking Data.");
    }
    if( reads > 0 && config.debug ){
      Serial.print("Temperature = ");
      Serial.print(tempature/100);
      Serial.print(" *C / ");
      Serial.print(1.8 * (tempature/100) + 32);
      Serial.println(" *F");
      
      Serial.print("Pressure = ");
      Serial.print(pressure / 100.0F);
      Serial.print(" hPa / ");
      Serial.print(pressure / 3386.0F);
      Serial.println(" inHg");
      Serial.print("Humidity = ");
      Serial.print(humidity / 1000.0F);
      Serial.println(" %");
    }
    reads++;
  }
  else{
    if( config.debug )
      Serial.print(F("- Initializing BME680 sensor\n"));
    BME_FOUND = sensor->begin(I2C_STANDARD_MODE);
    if( BME_FOUND ){
      if( config.debug )
        Serial.print(F("- Setting 16x oversampling for all sensors\n"));
      sensor->setOversampling(TemperatureSensor, Oversample16);  // Use enumerated type values
      sensor->setOversampling(HumiditySensor, Oversample16);     // Use enumerated type values
      sensor->setOversampling(PressureSensor, Oversample16);     // Use enumerated type values
      if( config.debug )
        Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
      sensor->setIIRFilter(IIR4);  // Use enumerated type values
      if( config.debug )
        Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n"));  
      sensor->setGas(320, 150);
    }
    else{
      Serial.print(F("-  Unable to find BME680. Trying again in 5 seconds.\n"));
    }
  }
}
