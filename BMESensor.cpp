
#include "BMESensor.h"

BMESensor::BMESensor(){
  sensor = new BME680_Class();
  BME_FOUND = false;
  reads = 0;
}
bool BMESensor::have_data(){
  return BME_FOUND;
}
int BMESensor::get_tempature(){
  return tempature;
}
int BMESensor::get_tempatureF(){
  return 1.8 * (tempature/100) + 32;
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
  if( BME_FOUND ){
    sensor->getSensorData(tempature, humidity, pressure, gas, false);  // Get readings
    if( reads > 0 ){
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
    Serial.print(F("- Initializing BME680 sensor\n"));
    BME_FOUND = sensor->begin(I2C_STANDARD_MODE);
    if( BME_FOUND ){
      Serial.print(F("- Setting 16x oversampling for all sensors\n"));
      sensor->setOversampling(TemperatureSensor, Oversample16);  // Use enumerated type values
      sensor->setOversampling(HumiditySensor, Oversample16);     // Use enumerated type values
      sensor->setOversampling(PressureSensor, Oversample16);     // Use enumerated type values
      Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
      sensor->setIIRFilter(IIR4);  // Use enumerated type values
      Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n"));  
      sensor->setGas(320, 150);
    }
    else{
      Serial.print(F("-  Unable to find BME680. Trying again in 5 seconds.\n"));
    }
  }
}
