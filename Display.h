#define FASTLED_ESP32_I2S true
#include <FastLED.h>

class Display {
 public:
  Display();
  ~Display() {}
  CRGB *getPixels();
  void setup(uint8_t digits, int show_pixels=22+18+18, uint8_t pixel_per_segment=9, uint8_t pixel_per_dot=5);
  void handle_display(int loop_counter);
  void digit(int number,int start_offset);
  void dots(struct CHSV color);
  void set_display(int hour, int minute, int second);
  void clear_digit(int start_offset);
  void show_temp(bool celsius);
  void allOff();
  void set_brightness(int bightness){current_bightness=bightness;};
  int get_brightness(){return current_bightness;};
  static void loop(void * parameter);
  void show_humidity();
  struct CHSV active_color(bool debug=false);

 private:
  CRGB *pixels;
  int show_pixels;
  int num_pixels;
  bool bracket=false;
  uint8_t pixel_per_segment;
  uint8_t pixel_per_dot;
  uint8_t digits;
  uint8_t loop_second;
  uint8_t current_bightness =5;
  uint8_t show_brightness;
  uint8_t color_set[8] = {0, 192, 64, 96, 160, 128, 32, 224};
  
  uint8_t labelHue = 0;
  uint8_t currentPatternNumber = 0;
  void nextPattern();
  
  // List of patterns to cycle through.  Each is defined as a separate function below.
  typedef void (Display::*PatternList)();
  PatternList showPatterns[8];
  PatternList transPatterns[8];
  
  void rotate_color();
  void rainbow();
  void rainbowWithGlitter();
  void addGlitter( fract8 chanceOfGlitter);
  void confetti();
  void sinelon();
  void bpm();
  void juggle();
};
