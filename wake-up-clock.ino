/*
 * Notes
 *
 * Need to check tweening values
 * Tweening could be queued up/re-written...
 * Helper to set the correct tween params, it's actually:
 *   easeFFF( pos, from, to - from, duration )
 * where:
 *   pos = current frame i.e. the millisecond/second/animation frame/loop iteration/etc
 *   from = starting value
 *   to - from = the amount to change by to get to the final value
 *   duration = how many steps the tween takes in total. It's nice because if you're
 *              using an animation that is time dependant, you can advance the position
 *              along to the curve to wherever you're supposed to be
 *              i.e 0 -> 1 -> lag -> 5 -> ..
 *
 * Nice to have functionality:
 *  - 'go to sleep' sunset lighting
 *  - moon lighting?
 *  - User selectable colour light
 *    - Could tween to the next colour
 * 
 * Future
 * - Extra satellite units could recieve radio input to fade lights. Could mean placing
 *   multiple units around a room and having them all sunrise in unison
 */

#define DEBUG
#define LCD_ENABLED

#define TOP 252

#define TEST_BUTTON 5
#define NEO_PIN 6
#define NEO_NUMPIX 8

// 0 = pin D2, 1 = pin D3
#define RTC_INTERRUPT 0

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <RTClib.h>
#include <SerialCommand.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include <Adafruit_NeoPixel.h>
//#include <Easing.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

RTC_DS1307 rtc;

#ifdef LCD_ENABLED
#define I2C_ADDR_LCD 0x38
LiquidCrystal_I2C lcd(I2C_ADDR_LCD);
#endif

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEO_NUMPIX, NEO_PIN, NEO_GRB + NEO_KHZ800);

SerialCommand sCmd;

boolean update_enabled = false;

void isr_rtc_interrupt() {
  update_enabled = true;
}

#ifdef LCD_ENABLED
#define NUM_CUSTOM_GLYPHS 5
byte glyphs[NUM_CUSTOM_GLYPHS][8] = {
  { B11111, B11111, B00000, B00000, B00000, B00000, B00000, B00000 },
  { B00000, B00000, B00000, B00000, B00000, B00000, B11111, B11111 },
  { B11111, B11111, B00000, B00000, B00000, B00000, B11111, B11111 },
  { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 },
  { B00000, B00000, B01110, B01110, B01110, B01110, B00000, B00000 }
};

const int digitWidth = 3;

// 32 is a space
const char bigDigitsTop[11][digitWidth] = {
  {3 ,0 ,3 },
  {0 ,3 ,32},
  {2 ,2 ,3 },
  {0 ,2 ,3 },
  {3 ,1 ,3 },
  {3 ,2 ,2 },
  {3 ,2 ,2 },
  {0 ,0 ,3 },
  {3 ,2 ,3 },
  {3 ,2 ,3 },
  {32,4 ,32}
};

const char bigDigitsBottom[11][digitWidth] = {
  {3 ,1 ,3 },
  {1 ,3 ,1 },
  {3 ,1 ,1 },
  {1 ,1 ,3 },
  {32,32,3 },
  {1 ,1 ,3 },
  {3 ,1 ,3 },
  {32,32,3 },
  {3 ,1 ,3 },
  {1 ,1 ,3 },
  {32,4 ,32}
};
#endif

char buffer[12];
boolean alarm_triggered = false;

struct Color { // RGB color structure
  byte r;
  byte g;
  byte b;
} color;

struct HSL { // HSL color (hue-saturation-lightness)
  byte h;
  byte s;
  byte l;
} hslcolor;

struct RgbTween {
  uint8_t from_r;
  uint8_t from_g;
  uint8_t from_b;
  uint8_t to_r;
  uint8_t to_g;
  uint8_t to_b;
  uint32_t duration;
  uint8_t  pos;
  boolean complete = true;
} tween;

DateTime alarm(2000,1,1,6,0,0);
uint16_t alarm_time = 0;

uint8_t easeInOutCubic (float t, float b, float c, float d) {
  if (c == 0) return (uint8_t) b;
  if ((t/=d/2) < 1) return c/2*t*t*t + b;
  return (uint8_t) c/2*((t-=2)*t*t + 2) + b;
}

void pad(uint8_t value) {
  if (value < 10) {
    Serial.print("  ");
  }
  else if (value < 100) {
    Serial.print(" ");
  }
}

void print_time( DateTime *datetime, String label ) {
      Serial.print(label);
      Serial.print( ": " );
      if (datetime->hour() < 10) Serial.print( "0" );
      Serial.print( datetime->hour() );
      Serial.print( ":" );
      if (datetime->minute() < 10) Serial.print( "0" );
      Serial.println( datetime->minute() );
}

/*
    This gets set as the default handler, and gets called when
    no other command matches.
*/
void cmd_unrecognised(const char *command) {
  Serial.println("Unrecognised command. Ensure you have newline enabled");
}

void cmd_alarm() {
  char *arg;
  arg = sCmd.next();

  String arg1 = String(arg);

  if (arg1 != NULL) {
    if (arg1.equalsIgnoreCase("GET")) {
      print_time( &alarm, "Alarm" );
    }
    else if (arg1.equalsIgnoreCase("SET")) {
      String arg2 = String(sCmd.next());
      // We are expecting a time next, 5 chars = 00:00
      if (arg2.length() == 5) {

        uint8_t hours   = (int) arg2.substring(0,2).toInt();
        uint8_t minutes = (int) arg2.substring(3,5).toInt();

        DateTime tempAlarm( 2000,1,1,hours,minutes );
        DateTime newAlarm( tempAlarm.unixtime() - 1200L );

        alarm = newAlarm;
        print_time( &alarm, "Alarm" );

        EEPROM_writeAnything(0, alarm);
        alarm_time = (alarm.hour()*100)+alarm.minute();
      }
    }
  }
  else {
    print_time( &alarm, "Alarm" );
  }
}

void cmd_time() {
  char *arg;
  arg = sCmd.next();

  String arg1 = String(arg);

  uint16_t year = 2000;
  uint8_t month = 1;
  uint8_t dd = 1;
  uint8_t hh = 0;
  uint8_t mm = 0;
  uint8_t ss = 0;

  if (arg1 != NULL) {
    if (arg1.equalsIgnoreCase("GET")) {
      DateTime now = rtc.now();
      print_time( &now, "Time" );
    }
    else if (arg1.equalsIgnoreCase("SET")) {
      String sDate = String(sCmd.next());
      String sTime = String(sCmd.next());

      // We are expecting a date, 2013-01-01, 10 chars long

      if (sDate.length() == 10) {
        year  = (uint16_t) sDate.substring(0,4).toInt();
        month = (uint8_t)  sDate.substring(6,8).toInt();
        dd    = (uint8_t)  sDate.substring(9,10).toInt();
      }

      // We are expecting a time next, so 8 chars long = 00:00:00
      if (sTime.length() == 8) {
        hh = (uint8_t) sTime.substring(0,2).toInt();
        mm = (uint8_t) sTime.substring(3,5).toInt();
        ss = (uint8_t) sTime.substring(6,8).toInt();
      }

      DateTime newTime( year,month,dd,hh,mm,ss );

//      Serial.println ( newAlarm.unixtime() );
//      Serial.println ( (uint8_t) newAlarm.unixtime() & 0xFF );

        rtc.adjust( newTime );
      }
    }
}

void cmd_color() {
  char *arg;
  arg = sCmd.next();

  String arg1 = String(arg);

  if (arg1 != NULL) {
    if (arg1.equalsIgnoreCase("GET")) {
      // return the current colour values
      Serial.print("RGB ");
      Serial.print(color.r);
      Serial.print(",");
      Serial.print(color.g);
      Serial.print(",");
      Serial.println(color.b);
    }
    else if (arg1.equalsIgnoreCase("RGB")) {
      // set the RGB values directly
      arg = sCmd.next();
      arg1 = String(arg);

      uint8_t comma1 = arg1.indexOf(",");
      uint8_t comma2 = arg1.indexOf(",", comma2);

      Color newcolor;

      newcolor.r = arg1.substring(0, comma1).toInt();
      newcolor.g = arg1.substring(comma1, comma2).toInt();
      newcolor.b = arg1.substring(comma2).toInt();

      color = newcolor;
    }
    else if (arg1.equalsIgnoreCase("HSL")) {
      // set the HSL values directly
      arg = sCmd.next();
      arg1 = String(arg);

      uint8_t comma1 = arg1.indexOf(",");
      uint8_t comma2 = arg1.indexOf(",", comma2);

      HSL newhsl;

      newhsl.h = arg1.substring(0, comma1).toInt();
      newhsl.s = arg1.substring(comma1, comma2).toInt();
      newhsl.l = arg1.substring(comma2).toInt();

      color = hslToRgb(&newhsl);
    }
  }
}

// A copy of color will be taken at the start of each sunrise update
static Color current_color = color;

struct Color hslToRgb(struct HSL *hsl) {

  Color rgb; // this will be the output

  if (hsl->s == 0) {
    rgb.r = hsl->l;
    rgb.g = hsl->l;
    rgb.b = hsl->l;

    return rgb;
  }

  if (hsl->l == 0) {
     rgb.r = 0;
     rgb.g = 0;
     rgb.b = 0;

     return rgb;
  }

  word temp; // temporary two-byte integer number for calculations

  byte top = hsl->l;
  temp = (top * (255 - hsl->s));
  byte bottom = (byte) (temp/255); // temp >> 8 is wrong

  byte cont = top - bottom;    // something like "contrast", but it isn't

  byte mod     = hsl->h % 42;  // 42 * 6 = 252, so 252 is the maximal hue
  byte segment = hsl->h / 42;  // division by power of two would be
                               //     some 200 times faster
  temp = mod * cont;           // here it comes a big product, so our
                               //     temp is needed
  byte x = (byte) (temp / 42); // scale it down

  // to understand this part, please refer to:
  // http://en.wikipedia.org/wiki/HSL_and_HSV#From_HSV
  switch (segment) {
    case 0:
      rgb.r = top;
      rgb.g = bottom + x;
      rgb.b = bottom;
      break;
    case 1:
      rgb.r = top - x;
      rgb.g = top;
      rgb.b = bottom;
      break;
    case 2:
      rgb.r = bottom;
      rgb.g = top;
      rgb.b = bottom + x;
      break;
    case 3:
      rgb.r = bottom;
      rgb.g = top - x;
      rgb.b = top;
      break;
    case 4:
      rgb.r = bottom + x;
      rgb.g = bottom;
      rgb.b = top;
      break;
    case 5:
      rgb.r = top;
      rgb.g = bottom;
      rgb.b = top - x;
      break;
    default: // something is wrong (hue is more than 252)
      rgb.r = 0;
      rgb.g = 0;
      rgb.b = 0;
  }

  return rgb; // here you go!
}

uint8_t last_mins = 0; // Last displayed minutes
uint8_t last_hour = 0; // Last displayed hour

void showDigit(int digit, int position, int extra_offset) {
  int x = position * (digitWidth + 1) + extra_offset;

  lcd.setCursor(x, 0);
  for (int i=0; i<digitWidth; i++) {
    lcd.write( bigDigitsTop[digit][i] );
  }

  lcd.setCursor(x, 1);
  for (int i=0; i<digitWidth; i++) {
    lcd.write( bigDigitsBottom[digit][i] );
  }
}

void showNumber(int value, int position, int extra_offset) {
  int index;
  itoa(value, buffer, 10);
  for (index=0; index<10; index++) {
    char c = buffer[index];
    if (c==0) {
      return;
    }
    c -= 48;
    showDigit(c, position + index, extra_offset);
  }
}

void showColon(boolean showHide) {
  if (showHide) {
    lcd.setCursor( 7, 0 );
    lcd.write( 4 );
    lcd.setCursor( 7, 1 );
    lcd.write( 4 );
  }
  else {
    lcd.setCursor( 7, 0 );
    lcd.write( 32 );
    lcd.setCursor( 7, 1 );
    lcd.write( 32 );
  }
}

DateTime last_time = (2000,1,1,0,0,0);
void showTime( DateTime *now ) {
  if (now->unixtime() == last_time.unixtime()) return;

  last_time = *now;

  // If the time hasn't changed, no need to update the display
  if (now->hour() != last_hour) {
    last_hour = now->hour();
    if (now->hour() < 10) {
      showNumber( 0, 0, 0 );
      showNumber( now->hour(), 1, 0 );
    }
    else {
      showNumber( now->hour(), 0, 0 );
    }
  }

  // Blinking colon for seconds display!
  showColon( now->second() & 1 );

  if (now->minute() != last_mins) {
    last_mins = now->minute();
    if (now->minute() >= 10) {
      showNumber( now->minute(), 2, 0 );
    }
    else {
      showNumber( 0, 2, 0 );
      showNumber( now->minute(), 3, 0 );
    }
  }

}

uint8_t  phase                = 0;
uint16_t sunrise_counter      = 0;
uint32_t sunrise_last_invoked = 0;

/*
 * We want a new way to light up:
 * 1. Start with one pixel set to lowest red
 * 2. Increase number of pixels at lowest red
 *    Light intensity is not linear, lets try doubling.
 *    So: 1 -> 2 -> 4 -> 8, and so on
 * 3. Move the hue by one luminance % 15, also increase luminance
 *    until luminance >= 150
 * 4. Move the hue until it hits 32
 * 5. Decrease saturation to 127
 * 6. Increase lumincance to 255
 * 7. Decrease saturation to 0
 */

void update_sunrise_tween( uint32_t timenow ) {
  if (!tween.complete) return;
  
  tween.complete = 0;
  tween.pos = 0;
  
  switch (phase) {
    case 0:
      hslcolor.h = 4;
      hslcolor.s = 255;
      hslcolor.l = 0;

      color = hslToRgb(&hslcolor);

      hslcolor.l = 1;
      
      /*
      tween.to_r = color.r;
      tween.to_g = color.g;
      tween.to_b = color.b;
      */

      tween.to_r = 0;
      tween.to_g = 0;
      tween.to_b = 0;
      
      tween.duration = 2;
      
      break;
    case 1:
      hslcolor.h = 15;
      hslcolor.l = 150;
      
      tween.duration = 240;
      
      break;
    case 2:
      hslcolor.h = 32;
      
      tween.duration = 180;

      break;
    case 3:
      hslcolor.s = 127;
      
      tween.duration = 240;

      break;
    case 4:
      hslcolor.l = 255;
      
      tween.duration = 240;

      break;
    case 5:
      hslcolor.s = 0;
      
      tween.duration = 720;

      break;
    case 6:
      alarm_triggered = false;
      break;
  }

  tween.from_r = color.r;
  tween.from_g = color.g;
  tween.from_b = color.b;

  color = hslToRgb(&hslcolor);
  
  tween.to_r = color.r - tween.from_r;
  tween.to_g = color.g - tween.from_g;
  tween.to_b = color.b - tween.from_b;
  
  phase++;
}

void setup() {
//#ifdef DEBUG
  Serial.begin(9600); // This speed to communicate with bluetooth module
//#endif
  Wire.begin();

#ifdef LCD_ENABLED
  // Set up the LCD
  lcd.begin(16, 2);

  // Set up custom characters
  for (int i=0;i<NUM_CUSTOM_GLYPHS;i++) {
    lcd.setCursor(0,1);
    lcd.write( i+1 );
    lcd.createChar(i, glyphs[i]);
  }
  lcd.clear();
#endif

  rtc.begin();
#ifdef LCD_ENABLED
  if (! rtc.isrunning()) {
    lcd.print("RTC is NOT running!");
  }
  else {
    lcd.print("RTC is running");
    lcd.setCursor(0,1);
    DateTime time = rtc.now();
    lcd.print(time.hour());
    lcd.print(':');
    lcd.print(time.minute());
  }
#endif

  // Set up the button pin
  pinMode( TEST_BUTTON, INPUT );

  // Initialise start condition for sunrise
  hslcolor.h = 0;
  hslcolor.s = 255;
  hslcolor.l = 0;

  sCmd.addCommand("ALARM", cmd_alarm);
  sCmd.addCommand("TIME",  cmd_time );
  sCmd.setDefaultHandler(cmd_unrecognised);

  EEPROM_readAnything(0, alarm);
  alarm_time = (alarm.hour()*100)+alarm.minute();

  pixels.begin();
  pixels.show();

#ifdef LCD_ENABLED
  lcd.clear();
#endif

  attachInterrupt(RTC_INTERRUPT, isr_rtc_interrupt, RISING);

  // Start the 1 second pulse
  rtc.writeSqwPinMode(SquareWave1HZ);
}

void loop() {

  // This was another likely culprit in the abnormal colour change stepping
  if (update_enabled) {
    update_enabled = false;

    DateTime now = rtc.now();
#ifdef LCD_ENABLED
    showTime( &now );
#endif

    if (alarm_triggered == false) {
      uint16_t nowtime = (now.hour()*100) + now.minute();
      if (nowtime == alarm_time) {
        Serial.println("Sunrise triggered via alarm");
        alarm_triggered = true;
      }
      else if (digitalRead(TEST_BUTTON) == 0) {
        Serial.println("Sunrise triggered via button");
        alarm_triggered = true;
      }
    }
    if (alarm_triggered == true) {
      update_sunrise_tween( now.unixtime() );

    /*
     * a trimmer resistor voltage divider can be connected
     * to the analog input so the color can be set up here.
     * please note, that maximal value for hue is 252, not 255.
     * larger values than 252 will produce RGB(0,0,0) output
     */

#ifdef DEBUG
      /*
       * Debug output on the serial console
      */
      Serial.print("DEBUG: H: ");
      pad(hslcolor.h);
      Serial.print(hslcolor.h, DEC);
      Serial.print(" S: ");
      pad(hslcolor.s);
      Serial.print(hslcolor.s, DEC);
      Serial.print(" L: ");
      pad(hslcolor.l);
      Serial.print(hslcolor.l, DEC);

      Serial.print("\t R: ");
      pad(color.r);
      Serial.print(color.r, DEC);
      Serial.print(" G: ");
      pad(color.g);
      Serial.print(color.g, DEC);
      Serial.print(" B: ");
      pad(color.b);
      Serial.println(color.b, DEC);
#endif

      uint8_t tween_r = easeInOutCubic(tween.pos, tween.from_r, tween.to_r, tween.duration);
      uint8_t tween_g = easeInOutCubic(tween.pos, tween.from_g, tween.to_g, tween.duration);
      uint8_t tween_b = easeInOutCubic(tween.pos, tween.from_b, tween.to_b, tween.duration);

      //if (tween.from_r == tween.to_r) tween_r = tween.to_r;
      //if (tween.from_g == tween.to_g) tween_g = tween.to_g;
      //if (tween.from_b == tween.to_b) tween_b = tween.to_b;

      Serial.print("Red tween: pos=");
      Serial.print(tween.pos, DEC);
      Serial.print(", from_r=");
      Serial.print(tween.from_r, DEC);
      Serial.print(", to_r=");
      Serial.print(tween.to_r);
      Serial.print(", duration=");
      Serial.print(tween.duration, DEC);
      Serial.print(", tween value=");
      Serial.print(easeInOutCubic(tween.pos, tween.from_r, tween.to_r, tween.duration));
      Serial.print(", tween_r=");
      Serial.println(tween_r, DEC);
      


      if (tween.pos == tween.duration) tween.complete = true;
      
      tween.pos++;

      for (int i=0; i < NEO_NUMPIX; i++) {
        // Get the tween values for r,g and b, and send them out
        // Could also tween the number of lit lights?

        pixels.setPixelColor(i, pixels.Color(tween_r, tween_g, tween_b));
      }
      pixels.show();
    }
  }

  sCmd.readSerial();
}

/*
  hue = 4
  luminance 0..+15
  hue + 1
  until luminance = 150
  hue +1 to 42
  saturation -1 to 0
*/
