/*
 * Notes
 * 
 * - Maybe use the 1hz output from the clock to trigger display
 *   updates and sunrise changes
 * - Might need to change button1 to another pin (so we can use
 *   the 1hz output from the clock as an external interrupt)
 */

#define DEBUG0

#define TOP 252

#define BUTTON1 2
#define NEO_PIN 8
#define NEO_NUMPIX 12

//#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <RTClib.h>
#include <SerialCommand.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

RTC_DS1307 RTC;

#define I2C_ADDR_LCD 0x38
LiquidCrystal_I2C lcd(I2C_ADDR_LCD);

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEO_NUMPIX, NEO_PIN, NEO_GRB + NEO_KHZ800);

SerialCommand sCmd;

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

char buffer[12];
byte mode;
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


DateTime alarm(2000,1,1,6,0,0);
uint16_t alarm_time = 0;

/*
    This gets set as the default handler, and gets called when
    no other command matches.
*/
void unrecognized(const char *command) {
  Serial.println("Unrecognised command. Ensure you have newline enabled");
}

void printAlarm( DateTime *alarm ) {
      Serial.print( "Alarm: " );
      if (alarm->hour() < 10) Serial.print( "0" );
      Serial.print( alarm->hour() );
      Serial.print( ":" );
      if (alarm->minute() < 10) Serial.print( "0" );
      Serial.println( alarm->minute() );
}

void processAlarm() {
  char *arg;
  arg = sCmd.next();

  String arg1 = String(arg);

  if (arg1 != NULL) {
    if (arg1.equalsIgnoreCase("GET")) {
      printAlarm( &alarm );
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
        printAlarm( &alarm );

        EEPROM_writeAnything(0, alarm);
        alarm_time = (alarm.hour()*100)+alarm.minute();
      }
    }
  }
  else {
    printAlarm( &alarm );
  }
}

void processTime() {
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
//    if (arg1.equalsIgnoreCase("GET")) {
//      printAlarm( &alarm );
//    }
//    else if (arg1.equalsIgnoreCase("SET")) {
    if (arg1.equalsIgnoreCase("SET")) {
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

        RTC.adjust( newTime );
      }
    }
}


void setup() {
//#ifdef DEBUG
  Serial.begin(57600); // for debug purposes
//#endif

  // Set up the LCD
  lcd.begin(16, 2);

  // Set up custom characters
  for (int i=0;i<NUM_CUSTOM_GLYPHS;i++) {
    lcd.setCursor(0,1);
    lcd.write( i+1 );
    lcd.createChar(i, glyphs[i]);
  }
  lcd.clear();

  Wire.begin();

  RTC.begin();
  if (! RTC.isrunning()) {
    lcd.print("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
//    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  else {
    lcd.print("RTC is running");
    lcd.setCursor(0,1);
    DateTime time = RTC.now();
    lcd.print(time.hour());
    lcd.print(':');
    lcd.print(time.minute());
    Serial.println( __DATE__ );
    Serial.println( __TIME__ );
  }
  delay(200);

  // Set up the button pin
  pinMode( BUTTON1, INPUT );

  // Initialise variables for reset condition
  mode = 0x00;

  // Initialise start condition for sunrise
  hslcolor.h = 0;
  hslcolor.s = 255;
  hslcolor.l = 0;

  sCmd.addCommand("ALARM", processAlarm);
  sCmd.addCommand("TIME",  processTime );
  sCmd.setDefaultHandler(unrecognized);

  EEPROM_readAnything(0, alarm);
  alarm_time = (alarm.hour()*100)+alarm.minute();

  pixels.begin();
  pixels.show();

  // Enable interrupts
  //sei();

  lcd.clear();
}

/* -----------------------------------------------------
 * Timer1 overflow interrupt
 * F_CPU 9.600.000 Hz
 * -> prescaler 0, overrun 256 -> 37.500 Hz
 * -> 256 steps -> 146 Hz
 *
 * Set global r, g and b to control the color.
 * Range is for rgb is 0-255 (dark to full power)
 */
static uint8_t count = 0;

// A copy of color will be taken at the start of each PWM phase
static Color current_color = color;

struct Color hslToRgb(struct HSL *hsl) {
/*
  // not exactly, but something similar
  // ^ pass by reference:
  // the paramter is the memory address of the variable
  // instead of value of the variable itself. it's  faster and
  // consumes less memory. please note that the members of this
  // structure can be accessed using the "->" arrow operator
  // instead of the usual "." dot.
*/
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

  byte mod = hsl->h % 42;      // 42 * 6 = 252, so 252 is the maximal hue
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


/*
  Phase 0: hue = 4
  Phase 1: hue + 1, luminance ..+15 until luminance = 150
  Phase 2: hue +1 to 42 (should be yellow)
  Phase 3: saturation -1 to 0
*/
void simulate_sunrise( uint32_t timenow ) {
  if (timenow - sunrise_last_invoked >= 4) {

    sunrise_last_invoked = timenow;

    switch (phase) {
      case 0:
        hslcolor.h = 4;
        phase++;
        break;
      case 1:
        hslcolor.l++;
        if (hslcolor.l % 15 == 0) {
          hslcolor.h++;
        }
        if (hslcolor.l == 150)
          phase++;
        break;
      case 2:
        hslcolor.h++;
        if (hslcolor.h == 32)
          phase++;
        break;
      case 3:
        hslcolor.s--;
        if (hslcolor.s == 127)
          phase++;
        break;
      case 4:
        hslcolor.l++;
        if (hslcolor.l == 255)
          phase++;
        break;
      case 5:
        hslcolor.s--;
        if (hslcolor.s == 0)
          phase++;
        break;
      case 6:
        alarm_triggered = false;
        break;
    }

    color = hslToRgb(&hslcolor);
  }
}



void loop() {

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
  Serial.print("\t H: ");
  Serial.print(hslcolor.h, DEC);
  Serial.print("\t S: ");
  Serial.print(hslcolor.s, DEC);
  Serial.print("\t L: ");
  Serial.print(hslcolor.l, DEC);

  Serial.print("\t R: ");
  Serial.print(color.r, DEC);
  Serial.print("\t G: ");
  Serial.print(color.g, DEC);
  Serial.print("\t B: ");
  Serial.println(color.b, DEC);
#endif


  // This was another likely culprit in the abnormal colour change stepping
  delay(200); // spend some time here to slow things down

  DateTime now = RTC.now();
  showTime( &now );

  if (alarm_triggered == false) {
    uint16_t nowtime = (now.hour()*100) + now.minute();
//    Serial.println(nowtime);
//    Serial.println(alarm_time);
    if (nowtime == alarm_time) {
      Serial.println("Sunrise triggered via alarm");
      alarm_triggered = true;
    }
    else if (digitalRead(BUTTON1) == 0) {
      Serial.println("Sunrise triggered via button");
      alarm_triggered = true;
    }
  }
  if (alarm_triggered == true) {
    simulate_sunrise( now.unixtime() );
  }

  sCmd.readSerial();
}



/*

A 010101010101
B 001100110011

Positions:
AB
00
10
01
11
00
10
01
11

*/


/*
  hue = 4
  luminance ..+15
  hue + 1
  until luminance = 150
  hue +1 to 42
  saturation -1 to 0
*/
