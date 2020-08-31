#include <Arduino.h>
#include <U8g2lib.h>
#include <DS3231.h>
#include <Wire.h>          // For the i2c devices
#include <string.h>

#define DS3231_I2C_ADDRESS 104    // RTC is connected, address is Hex68 (Decimal 104)
#define ON_TIME 9

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // Adafruit Feather ESP8266/32u4 Boards + FeatherWing OLED//U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);  // Adafruit ESP8266/32u4/ARM Boards + FeatherWing OLED


#define STANDBY 0//in this mode the time it is showed, also the wake up time
#define H_WTIME  1//in this mode the wake up time it is showed with the hour tilting so you
//can change the hour
#define M_WTIME  2//Minutes tilting so you can change minutes
//next state returns to STANDBY


byte seconds, minutes, hours, day, date, month, year;
byte temp_hours = 0, temp_minutes = 0;
char weekDay[4];
byte tMSB, tLSB;
char my_array[100];            // Character array for printing something.
int hour_blink = 0;
bool blink_bool = true;
bool alarm = false;
int bulb_state = HIGH;

const int BULB = 6; //pin of the activation of the red bulb

//for setting up the buttons for interruptions
const byte button1 = 2;
const byte button2 = 3;

//indicates if any of the buttons has been pressed
volatile boolean f1 = false;
volatile boolean f2 = false;


//indicates de actual state of the machine state
byte state = STANDBY;

// BUTTON 1 VARIABLES AND ISR
long buttonTimer1 = 0;
long longPressTime1 = 500;

boolean buttonActive1 = false;

//changes to the next mode
void fbutton1() {
  
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  static boolean falling=true; //with static this variable is only declared once
  
  // If interrupts come faster than 50ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 50)
  {
    
    if(falling==true){
      Serial.println("BUTTON1 PRESSED");
      f1 = true;
      falling = false;
      attachInterrupt(digitalPinToInterrupt(button1), fbutton1, RISING);
    }
    else{
      Serial.println("BUTTON1 DROPPED");
      Serial.println("");
      falling = true;
      attachInterrupt(digitalPinToInterrupt(button1), fbutton1, FALLING);
    }
      
  }
  last_interrupt_time = interrupt_time;
  
}

// BUTTON 2 VARIABLES AND ISR
long buttonTimer2 = 0;
long longPressTime2 = 500;

boolean buttonActive2 = false;
boolean longPressActive2 = false;

volatile int button2c = 0;
void fbutton2() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  static boolean falling=true; //with static this variable is only declared once
  
  // If interrupts come faster than 50ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 50)
  {
    
    if(falling==true){
      Serial.println("BUTTON2 PRESSED");
      f2 = true;
      falling = false;
      attachInterrupt(digitalPinToInterrupt(button2), fbutton2, RISING);
    }else{
      Serial.println("BUTTON2 DROPPED");
      Serial.println("");
      falling = true;
      attachInterrupt(digitalPinToInterrupt(button2), fbutton2, FALLING);
    }
      
  }
  last_interrupt_time = interrupt_time;
}


// OLED FUNCTION
void print_oled(const char * a) {
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  u8g2.drawStr(0, 15, a); // write something to the internal memory
  u8g2.sendBuffer();          // transfer internal memory to the display
}


// DS3231 FUNCTIONS
// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return ( (val / 10 * 16) + (val % 10) );
}

void watchConsole()
{
  if (Serial.available()) {      // Look for char in serial queue and process if found
    if (Serial.read() == 84) {      //If command = "T" Set Date
      set3231Date();
      get3231Date();
      // Serial.println(" ");
    }
  }
}

void set3231Date()
{
  //T(sec)(min)(hour)(dayOfWeek)(dayOfMonth)(month)(year)
  //T(00-59)(00-59)(00-23)(1-7)(01-31)(01-12)(00-99)
  //Example: 02-Feb-09 @ 19:57:11 for the 3rd day of the week -> T1157193020209
  // T1124154091014
  seconds = (byte) ((Serial.read() - 48) * 10 + (Serial.read() - 48)); // Use of (byte) type casting and ascii math to achieve result.
  minutes = (byte) ((Serial.read() - 48) * 10 +  (Serial.read() - 48));
  hours   = (byte) ((Serial.read() - 48) * 10 +  (Serial.read() - 48));
  day     = (byte) (Serial.read() - 48);
  date    = (byte) ((Serial.read() - 48) * 10 +  (Serial.read() - 48));
  month   = (byte) ((Serial.read() - 48) * 10 +  (Serial.read() - 48));
  year    = (byte) ((Serial.read() - 48) * 10 +  (Serial.read() - 48));
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0x00);
  Wire.write(decToBcd(seconds));
  Wire.write(decToBcd(minutes));
  Wire.write(decToBcd(hours));
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(date));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));
  Wire.endTransmission();
}


void get3231Date()
{
  // send request to receive data starting at register 0
  Wire.beginTransmission(DS3231_I2C_ADDRESS); // 104 is DS3231 device address
  Wire.write(0x00); // start at register 0
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7); // request seven bytes

  if (Wire.available()) {
    seconds = Wire.read(); // get seconds
    minutes = Wire.read(); // get minutes
    hours   = Wire.read();   // get hours
    day     = Wire.read();
    date    = Wire.read();
    month   = Wire.read(); //temp month
    year    = Wire.read();

    seconds = (((seconds & B11110000) >> 4) * 10 + (seconds & B00001111)); // convert BCD to decimal
    minutes = (((minutes & B11110000) >> 4) * 10 + (minutes & B00001111)); // convert BCD to decimal
    hours   = (((hours & B00110000) >> 4) * 10 + (hours & B00001111)); // convert BCD to decimal (assume 24 hour mode)
    day     = (day & B00000111); // 1-7
    date    = (((date & B00110000) >> 4) * 10 + (date & B00001111)); // 1-31
    month   = (((month & B00010000) >> 4) * 10 + (month & B00001111)); //msb7 is century overflow
    year    = (((year & B11110000) >> 4) * 10 + (year & B00001111));
  }
  else {
    //oh noes, no data!
  }

  switch (day) {
    case 1:
      strcpy(weekDay, "Sun");
      break;
    case 2:
      strcpy(weekDay, "Mon");
      break;
    case 3:
      strcpy(weekDay, "Tue");
      break;
    case 4:
      strcpy(weekDay, "Wed");
      break;
    case 5:
      strcpy(weekDay, "Thu");
      break;
    case 6:
      strcpy(weekDay, "Fri");
      break;
    case 7:
      strcpy(weekDay, "Sat");
      break;
  }
}

float get3231Temp()
{
  float temp3231;

  //temp registers (11h-12h) get updated automatically every 64s
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0x11);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 2);

  if (Wire.available()) {
    tMSB = Wire.read(); //2's complement int portion
    tLSB = Wire.read(); //fraction portion

    temp3231 = (tMSB & B01111111); //do 2's math on Tmsb
    temp3231 += ( (tLSB >> 6) * 0.25 ); //only care about bits 7 & 8
  }
  else {
    //oh noes, no data!
  }

  return temp3231;
}

void setup() {

  //Wire.begin();
  u8g2.begin(); //OLED display
  pinMode(BULB, OUTPUT);  //bulb
  pinMode(button1, INPUT_PULLUP); //button for changing mode
  pinMode(button2, INPUT_PULLUP); //button for adding time
  attachInterrupt(digitalPinToInterrupt(button1), fbutton1, FALLING);
  attachInterrupt(digitalPinToInterrupt(button2), fbutton2, FALLING);
  Serial.begin(9600);
  // Serial.println("PROGRAM READY");
  digitalWrite(BULB, HIGH); //RELAY with na (normally open) works with inverse logic, need to be
  //to ground to be connected
}

void loop() {

  // DS3231 ACTIONS
  Wire.begin();
  watchConsole();
  get3231Date();
  Wire.end();

  switch (state) {

    case STANDBY:

      // ALARM LOGIC FOR ACTIVATING THE RELAY
      if (alarm == true) {

        if (temp_hours == hours && temp_minutes == minutes) {

          if (bulb_state == LOW) { // RELAY IS ON
            bulb_state = HIGH;
            Serial.print("new BULB_STATE: ");
            // Serial.println(bulb_state);
            noInterrupts();
            digitalWrite(BULB, bulb_state);
            cli();
            interrupts();
            alarm = false;

          } else {
            bulb_state = LOW;
            Serial.print("new BULB_STATE: ");
            // Serial.println(bulb_state);
            noInterrupts();
            digitalWrite(BULB, bulb_state);
            cli();
            interrupts();

            // ADD ANOTHER TIMER FOR TURNING OFF THE BULB
            if ((temp_minutes >= 60 - ON_TIME) && (temp_hours == 23)) {
              temp_minutes = (temp_minutes + ON_TIME) % 60;
              temp_hours = 0;
            } else if (temp_minutes >= 60 - ON_TIME) {
              temp_minutes = (temp_minutes + ON_TIME) % 60;
              temp_hours++;
            } else {
              temp_minutes += ON_TIME;
            }

          }

        }

        u8g2.clearBuffer();          // clear the internal memory
        u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font

        sprintf(my_array, "%02u:%02u", hours, minutes);
        u8g2.drawStr(0, 15, my_array); // write something to the internal memory

        sprintf(my_array, "ALARM - ON -> %02u:%02u", temp_hours, temp_minutes);
        u8g2.drawStr(0, 29, my_array); // write something to the internal memory

        u8g2.sendBuffer();          // transfer internal memory to the display
      } else {
        sprintf(my_array, "%02u:%02u ALARM - OFF", hours, minutes);
        print_oled(my_array);
      }



      //BUTTON LOGIC FOR CHANGING MODE OR ACTIVATING/DEACTIVATING THE ALARM
      //BUTTON 1
      if (digitalRead(button1) == LOW) { // first button pressed

        if (buttonActive1 == false) { //first time being pulsed

          buttonActive1 = true;
          buttonTimer1 = millis();

        }

        if (millis() - buttonTimer1 > longPressTime1) {

          // Serial.println("LONG PRESSED BUTTON");
          buttonActive1 = false;

          if (temp_hours == 0 && temp_minutes == 0) {
            temp_hours = hours;
            temp_minutes = minutes;
          }

          state = H_WTIME;
          f1 = false;
          f2 = false;
        }

      } else { // no button pressed

        if (buttonActive1 == true) {

          buttonActive1 = false;

        }

      }


      //BUTTON 2
      if (digitalRead(button2) == LOW) { // first button pressed

        if (buttonActive2 == false) { //first time being pulsed

          buttonActive2 = true;
          buttonTimer2 = millis();

        }

        if ((millis() - buttonTimer2 > longPressTime2) && (longPressActive2 == false)) {
          longPressActive2 = true;
          // Serial.println("LONG PRESSED BUTTON");
          buttonActive2 = false;
          alarm = !alarm;
        }

      } else { // no button pressed

        if (buttonActive2 == true) {

          buttonActive2 = false;
          longPressActive2 = false;

        }

      }

      break;


    case H_WTIME:
      //shows the wake up time with hour tilting
      hour_blink++;
      if (hour_blink % 2 == 0) {
        hour_blink = 0;
        blink_bool = ! blink_bool;
      }
      if (blink_bool) {
        sprintf(my_array, "SET HOUR - %02u:%02u", temp_hours, temp_minutes);
      } else {
        sprintf(my_array, "SET HOUR - __:%02u", temp_minutes);
      }

      // Serial.println(my_array);
      print_oled(my_array);


      if (f1) {

        state = M_WTIME;
        f1 = false;
        f2 = false;

      } else if (f2) {

        if (temp_hours == 23) {
          temp_hours = 0;
        } else {
          temp_hours++;
        }
        f1 = false;
        f2 = false;
      }

      break;


    case M_WTIME:

      //shows the wake up time with minute tilting
      hour_blink++;
      if (hour_blink % 2 == 0) {
        hour_blink = 0;
        blink_bool = ! blink_bool;
      }
      if (blink_bool) {
        sprintf(my_array, "SET MINUTE - %02u:%02u", temp_hours, temp_minutes);
      } else {
        sprintf(my_array, "SET MINUTE - %02u:__", temp_hours);
      }

      // Serial.println(my_array);
      print_oled(my_array);
      if (f1) {
        state = STANDBY;
        f1 = false;
        f2 = false;
      } else if (f2) {
        temp_minutes = (temp_minutes + 10) % 60;
        f2 = false;
      }

      break;
  }

  delay(10);

}
