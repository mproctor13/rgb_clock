
#include "Display.h"
#include "TimeUtil.h"
#include "BMESensor.h"
#define DATA_PIN 2

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

Display::Display(){
  show_brightness=255;
  
  showPatterns[0] = &Display::rotate_color;
  showPatterns[1] = &Display::rainbow;
  showPatterns[2] = &Display::rainbowWithGlitter;
  showPatterns[3] = &Display::confetti;
  showPatterns[4] = &Display::sinelon;
  showPatterns[5] = &Display::juggle;
  showPatterns[6] = &Display::bpm;
}

void Display::setup(uint8_t digits, int show_pixels, uint8_t pixel_per_segment, uint8_t pixel_per_dot){
  this->loop_second=0;
  num_pixels = show_pixels+digits*7*pixel_per_segment;
  this->pixel_per_segment = pixel_per_segment;
  this->pixel_per_dot = pixel_per_dot;
  this->digits = digits;
  if(this->digits == 6){
    num_pixels += pixel_per_dot*4;
  }
  else{
    num_pixels += pixel_per_dot*2;
  }
  this->show_pixels = show_pixels;
  
  Serial.print("Show Pixels: ");
  Serial.print(show_pixels);
  Serial.print(", Pixels per Segment: ");
  Serial.print(pixel_per_segment);
  Serial.print(", Pixels per Dot: ");
  Serial.print(pixel_per_dot);
  Serial.print(", Total number of Pixels: ");
  Serial.print(num_pixels);
  Serial.print(" on pin: ");
  Serial.println(DATA_PIN);
  Serial.print("Malloc of ");
  Serial.println(sizeof(CRGB) * num_pixels);
  pixels = (CRGB*) malloc(sizeof(CRGB) * num_pixels);
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(pixels, num_pixels);
  allOff();
  fill_solid(pixels, show_pixels, CRGB::White);
  FastLED.show();
}

void Display::allOff(){
  fill_solid(pixels, num_pixels, CRGB::Black);
}

void Display::loop(void * parameter) {
  int display_counter = 0;
  extern Display *clock_display;
  clock_display->setup(4);
  for(;;){ // infinite loop
    clock_display->handle_display(display_counter);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    display_counter += 20;
  }
}

void Display::handle_display(int loop_counter){
  extern TimeUtil timeutil;

  if(loop_counter % 1000 == 0){ // Every Second
    if(loop_second > 15 && loop_second <= 20){
      show_temp(false);
    }
    else if(loop_second > 20 && loop_second <= 25){
      show_humidity();
    }
    else{
      set_display(timeutil.clock_hour(), 
                  timeutil.clock_min(), 
                  timeutil.clock_sec());
    }
    if(loop_counter % 2000 == 0){
      dots( CHSV(0, 0, current_bightness) ); // Dots White
    }
    else{
      dots(active_color()); // Dots Primary Color
    }
    loop_second++;
  }
  if(loop_counter % 10000 == 0){ // Every 10 Seconds
    nextPattern();
  }
  if(loop_counter % 60000 == 0){ // Every Minute
    loop_second=0;
  }
  rotate_color();
  FastLED.show();
  labelHue++;
}

struct CHSV Display::active_color(bool debug){
  extern TimeUtil timeutil;
  uint8_t color = timeutil.clock_min()%8;
  if((timeutil.clock_hour() == 4 || 
      timeutil.clock_hour() == 16)  && 
      timeutil.clock_min() == 20){
    color = 96;
  }
  if( debug ){
    Serial.print("Current brightness: ");
    Serial.print(current_bightness);
    Serial.print("and color: ");
    Serial.println(color);
  }
  return CHSV(color_set[color], 255, current_bightness);
}

void Display::show_temp(bool celsius){
  extern BMESensor *bme;
  int tempature;
  int type_offset;

  if( bme->have_data() ){
    if( celsius ){
      tempature = bme->get_tempatureC();
    }
    else{
      tempature = bme->get_tempatureF();
    }
    fill_solid(pixels+show_pixels, num_pixels-show_pixels, CRGB::Black);
    if(tempature > 100){
      type_offset = show_pixels+21*pixel_per_segment+pixel_per_dot*2;
      digit(tempature/100, show_pixels);
      digit((tempature-100)/10, show_pixels+7*pixel_per_segment);
      digit(tempature%10, show_pixels+21*pixel_per_segment+pixel_per_dot*2);
    }
    else{
      digit(tempature/10, show_pixels);  Serial.print("show brightness: ");
//  Serial.println(t);
      digit(tempature%10, show_pixels+7*pixel_per_segment);
      type_offset = show_pixels+14*pixel_per_segment+pixel_per_dot*2;
    }
    if( celsius ){
      //MIDDLESEGMENT
      fill_solid(pixels+type_offset+6*pixel_per_segment, pixel_per_segment, active_color());
      
      //LOWERLEFTSEGMENT
      fill_solid(pixels+type_offset+2*pixel_per_segment, pixel_per_segment, active_color());
      
      //LOWERSEGMENT
      fill_solid(pixels+type_offset+1*pixel_per_segment, pixel_per_segment, active_color());
    }
    else{
      //UPPER SEGMENT
      fill_solid(pixels+type_offset+4*pixel_per_segment, pixel_per_segment, active_color());
      
      //UPPERLEFTSEGMENT
      fill_solid(pixels+type_offset+3*pixel_per_segment, pixel_per_segment, active_color());
      
      //MIDDLESEGMENT
      fill_solid(pixels+type_offset+6*pixel_per_segment, pixel_per_segment, active_color());
      
      //LOWERLEFTSEGMENT
      fill_solid(pixels+type_offset+2*pixel_per_segment, pixel_per_segment, active_color());
    }
  }
}

void Display::show_humidity(){
  extern BMESensor *bme;
  int humidity;
  int start_offset;

  if( bme->have_data() ){
    humidity = bme->get_humidity();
    fill_solid(pixels+show_pixels, num_pixels-show_pixels, CRGB::Black);
    digit(humidity/10, show_pixels);
    digit(humidity%10, show_pixels+7*pixel_per_segment);
    Serial.print("humidity:[");
    Serial.print(humidity/10);
    Serial.print("]");
    Serial.print("[");
    Serial.print(humidity%10);
    Serial.println("]");
    start_offset = show_pixels+14*pixel_per_segment+pixel_per_dot*2;
    //UPPER SEGMENT
    fill_solid(pixels+start_offset+4*pixel_per_segment, pixel_per_segment, active_color());

    //UPPERLEFTSEGMENT
    fill_solid(pixels+start_offset+3*pixel_per_segment, pixel_per_segment, active_color());
    
    //UPPERRIGHTSEGMENT
    fill_solid(pixels+start_offset+5*pixel_per_segment, pixel_per_segment, active_color());

    //MIDDLESEGMENT
    fill_solid(pixels+start_offset+6*pixel_per_segment, pixel_per_segment, active_color());

    start_offset = show_pixels+21*pixel_per_segment+pixel_per_dot*2;
    //MIDDLESEGMENT
    fill_solid(pixels+start_offset+6*pixel_per_segment, pixel_per_segment, active_color());

    //LOWERLEFTSEGMENT
    fill_solid(pixels+start_offset+2*pixel_per_segment, pixel_per_segment, active_color());

    //LOWERRIGHTSEGMENT
    fill_solid(pixels+start_offset+0*pixel_per_segment, pixel_per_segment, active_color());

    //LOWERSEGMENT
    fill_solid(pixels+start_offset+1*pixel_per_segment, pixel_per_segment, active_color());
  }
}

void Display::set_display(int hour, int minute, int second) {
  // ***************** Start Hour ***************** 
  Serial.print("[");
   if(hour > 9){
    Serial.print(hour/10);
    digit(hour/10, show_pixels);
  }
  else{
    Serial.print(" ");
    clear_digit(show_pixels);
  }
  Serial.print("]");
  
  Serial.print("[");
  if(hour != 0){
    Serial.print(hour%10);
    digit(hour%10, show_pixels+7*pixel_per_segment);
  }
  else{
    Serial.print(" ");
    clear_digit(show_pixels+7*pixel_per_segment);
  }
  Serial.print("]:");
  
  // ***************** End Hour ***************** 
  // ***************** Start Minute ***************
  Serial.print("[");
  Serial.print(minute/10);
  digit(minute/10, show_pixels+14*pixel_per_segment+pixel_per_dot*2);
  Serial.print("]");
  
  Serial.print("[");
  if(minute > 10){
    Serial.print(minute%10);
    digit(minute%10, show_pixels+21*pixel_per_segment+pixel_per_dot*2);
  }
  else{
    Serial.print(minute%10);
    digit(minute%10, show_pixels+21*pixel_per_segment+pixel_per_dot*2);
  }
  // *****************  End Minute ***************** 
  if( digits == 6 ){
    Serial.print("]");
    // ***************** Start Seconds ***************** 
    Serial.print(":[");
    Serial.print(second/10);
    digit(second/10, show_pixels+28*pixel_per_segment+pixel_per_dot*4);
    Serial.print("]");
    
    Serial.print("[");
    if(second > 9){
      Serial.print(second%10);
      digit(second%10, show_pixels+35*pixel_per_segment+pixel_per_dot*4);
    }
    else{
      Serial.print(second);
      digit(second, show_pixels+35*pixel_per_segment+pixel_per_dot*4);
    }
    // End Seconds
  }
  Serial.println("] ");
}


void Display::dots(struct CHSV color) {
//  fill_solid(&pixels[show_pixels], pixel_per_dot*2, color);
  for(int i=0; i<pixel_per_dot*2; i++){
    pixels[show_pixels+2*pixel_per_segment*7+i] = color;
  }
  if(digits == 6){
    for(int i=0; i<pixel_per_dot*2; i++){
      pixels[show_pixels+4*pixel_per_segment*7+pixel_per_dot*2+i] = color;
    }
//    fill_solid(&(pixels[show_pixels+4*pixel_per_segment*7]), pixel_per_dot*2, color);
  }
}

void Display::clear_digit(int start_offset) {
  fill_solid(pixels+start_offset, 7*pixel_per_segment, CRGB::Black);
}

void Display::digit(int number,int start_offset) {
  fill_solid(pixels+start_offset, pixel_per_segment*7, CRGB::Black);
//  Serial.print("Showing Number: ");
//  Serial.println(number);
  //UPPER SEGMENT
  if (number == 0 || number == 2 || number == 3 || number == 5 || 
        number == 6 || number == 7 || number == 8 || number == 9) {
    fill_solid(pixels+start_offset+4*pixel_per_segment, pixel_per_segment, active_color());
  }

  //UPPERLEFTSEGMENT
  if (number == 0 || number == 4 || number == 5 || 
        number == 6 || number == 8 || number == 9) {
    fill_solid(pixels+start_offset+3*pixel_per_segment, pixel_per_segment, active_color());
  }

  //UPPERRIGHTSEGMENT
  if (number == 0 || number == 1 || number == 2 || number == 3 || 
        number == 4 || number == 7 || number == 8 || number == 9) {
    fill_solid(pixels+start_offset+5*pixel_per_segment, pixel_per_segment, active_color());
  }

  //MIDDLESEGMENT
  if (number == 2 || number == 3 || number == 4 || 
        number == 5 || number == 6 || number == 8 || number == 9) {
    fill_solid(pixels+start_offset+6*pixel_per_segment, pixel_per_segment, active_color());
  }

  //LOWERLEFTSEGMENT
  if (number == 0 || number == 2 || number == 6 || number == 8) {
    fill_solid(pixels+start_offset+2*pixel_per_segment, pixel_per_segment, active_color());
  }

  //LOWERRIGHTSEGMENT
  if (number == 0 || number == 1 || number == 3 || number == 4 || 
        number == 5 || number == 6 || number == 7 || number == 8 || number == 9) {
    fill_solid(pixels+start_offset+0*pixel_per_segment, pixel_per_segment, active_color());
  }

  //LOWERSEGMENT
  if (number == 0 || number == 2 || number == 3 || 
        number == 5 || number == 6 || number == 8 || number == 9) {
    fill_solid(pixels+start_offset+1*pixel_per_segment, pixel_per_segment, active_color());
  }

  // This sends the updated pixel color to the hardware.

}

CRGB* Display::getPixels(){
  return pixels;
}

void Display::nextPattern(){
  // add one to the current pattern number, and wrap around at the end
//  currentPatternNumber = (currentPatternNumber + 1) % ARRAY_SIZE( patterns );
}

void Display::rotate_color(){
//  int t = int(125+float(current_bightness/255)*125.0);
//  Serial.print("show brightness: ");
//  Serial.println(t);
  if(bracket){
    fill_solid(pixels, show_pixels, CHSV(labelHue, 255, show_brightness));
  }
  else{
    fill_solid(pixels, 22, CHSV(labelHue, 255, show_brightness));
    fill_solid(pixels+22, 36, CHSV(0, 0, show_brightness)); // White
  }
}

void Display::rainbow(){
  // FastLED's built-in rainbow generator
  fill_rainbow(pixels, show_pixels, labelHue, 7);
}

void Display::rainbowWithGlitter(){
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void Display::addGlitter( fract8 chanceOfGlitter){
  if( random8() < chanceOfGlitter) {
    pixels[ random16(show_pixels) ] += CRGB::White;
  }
}

void Display::confetti(){
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(pixels, show_pixels, 10);
  int pos = random16(show_pixels);
  pixels[pos] += CHSV( labelHue + random8(64), 200, 255);
}

void Display::sinelon(){
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( pixels, 22, 20);
  int pos = beatsin16( 13, 0, 22-1 );
  pixels[pos] += CHSV( labelHue, 255, 192);
}

void Display::bpm(){
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < show_pixels; i++) { //9948
    pixels[i] = ColorFromPalette(palette, labelHue+(i*2), beat-labelHue+(i*10));
  }
}

void Display::juggle(){
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(pixels, show_pixels, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    pixels[beatsin16( i+7, 0, show_pixels-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}
