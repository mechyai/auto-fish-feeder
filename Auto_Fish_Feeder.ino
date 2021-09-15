//**Library Initialization**
#include <Servo.h>
#include <HX711.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

//**Object Initialization**
Servo servoDisp; //servo motor

//**Pin Initialization**
const byte ManualButtonPin = 0; //pin 2
const byte ModeButtonPin = 1; //pin 3
const byte DCmotorPin = 5; //PWM pin, only certain pins work wiht Servo library
const byte servoPin = 9; //Servo Motor PWM pin
HX711 scale(A1, A0); // DK = A1, SCK = A0
const byte photoPin = A2; //photoresistor pin
const byte dialPotPin = A3; //variable resistor dial pin
//LCD - SDA = A4 (Yellow); SCL = A5 (Orange);


LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); //set LCD IIC address


//**Value Initialization**
const byte Hole_open = 140; //open Servo position
const byte Hole_closed = 179; //closed Servo position
const int min_Light = 300; //Tank LED light level
const int cal_factor = 250; //found experiementally
//Timer and Water Temp
const byte temp_address = 72; //I2C temp address
byte times_fed = 0; //number of times fed
long time_sec = 0; //must have different references for each seperate timer
long time_temp = 0;
byte water_temp = 0;
long weight_refresh = 0;
//Time defaults
byte sec = 0;
byte minute = 0;
byte hour = 0;

boolean mode_toggle = true;
//boolean manual_toggle = true;

volatile int Mode = 1; //Interval/Standard - pin 3 --interrupter button
volatile boolean Manual_Feed = false; //pin 2 -- interrupter button

boolean display_reset = true; //default LCD screen refresh

//***********************************************************************************************

void setup() {
  //Serial.begin(9600);
  Wire.begin();

  pinMode(photoPin, INPUT);
  pinMode(dialPotPin, INPUT);
  pinMode(ModeButtonPin, INPUT);
  pinMode(ManualButtonPin, INPUT);

  pinMode(DCmotorPin, OUTPUT);
  servoDisp.attach(servoPin); //SERVO control

  attachInterrupt(ModeButtonPin, ModeDisplayButton, RISING);
  attachInterrupt(ManualButtonPin, ManualFeedButton, RISING);

  //load cell initialization
  scale.set_scale(cal_factor);
  scale.tare();

  lcd.backlight();
  lcd.begin(20, 4);
  lcd.setCursor(0, 0);
  lcd.print("Timer: ");
  lcd.setCursor(18, 0);
  lcd.print("oF");
  //lcd.print(char(0));
  //lcd.write(byte(1));
  lcd.setCursor(0, 2);
  lcd.print("Mode:");
  lcd.setCursor(0, 3);
  lcd.print("Food Lvl:");
  lcd.setCursor(0, 1);
  lcd.write("Weight: ");
  lcd.setCursor(18,3);
  lcd.write("*0");
//  lcd.setCursor(6, 2);
//  lcd.print("STANDARD");

}

//*************************************************************************************************

void loop() {
  sec = 0; minute = 0; hour = 0;
  FeedingTimer();
  
  while (analogRead(photoPin) > min_Light) //senses light from tank's LEDs
  {
    //LCD initialization
    lcd.on();
    lcd.backlight();   
    //**Run setup Methods**
    FeedingTimer(); //runs clock based on last feeding to LCD
     if (display_reset == true) //reset screen after LED ON/OFF
    {
      sec =0; minute=0; hour=0;
      scale.tare();
      delay(500);
      time_sec = time_sec + 500;
      lcd.clear();
      OriginalDisplay();
      display_reset = false; 
    }
    
    lcd.setCursor(19,3); //display how many times fish have been fed that day
    lcd.print(times_fed);   
    WaterTemp(); //displays water temp to LCD
    FeedingTimer(); //runs clock based on last feeding to LCD
    float food_val_amount = FoodAmountDial(); //selects amount of food dispensed based on DIAL
    double weight = scale.get_units(5); //must be small enough AVG not to disturb timing (<5)
    lcd.setCursor(8,1);
    lcd.print(weight); //prints current Scale reading to LCD
    if (millis() - weight_refresh >= 1750) //weight display refresh
    {
      weight_refresh += 1750;
      lcd.setCursor(8,1);
      lcd.print("        "); //get rid of overflow negative/long values
    }
    lcd.setCursor(8,1);
    lcd.print(weight); //redisplay weight
    
    //manual_feed = ModeDisplayButton(); //decide standard or interval mode, displayed on LCD
    if (hour == 0 && minute == 0 && sec == 0)
      OriginalDisplay();
    
    if (Mode == 4) //button change
    {
      lcd.setCursor(6,2);
      lcd.write("INTERVAL");
    }
    if (Mode == 1)
    {
      lcd.setCursor(6,2);
      lcd.write("STANDARD");
    }
        
    if (times_fed == 0 && minute == 10 && sec == 1) //Morning feeding
    {
      FeedFish(Mode, food_val_amount);
      sec = 0; minute = 0; hour = 0; //Reset feeding timer
      times_fed++;
      OriginalDisplay();
    }
    
    if (Manual_Feed == true) //F button pressed
    {
      FeedFish(Mode, food_val_amount);
      
      OriginalDisplay();
      times_fed++;
      sec = 0; minute = 0; hour = 0;
      Manual_Feed = false; //reset manual toggle button
    }
    
    if (times_fed <= 2 && hour == 4 && minute == 0 && sec == 1) //evening feeding: adjust HOUR to change timing
    {
      // run morning feeding
      FeedFish(Mode, food_val_amount);
      sec = 0; minute = 0; hour = 0; //Reset feeding timer
      OriginalDisplay();
      times_fed++;
    }

  }
  //LCD/System sleep mode == tank LED is OFF
  display_reset = true;
  lcd.off();
  times_fed = 0;
}

//***********************************************************************************************
//***User-Defined Functions***//

void FeedFish(int interval, int food_val)
{
  int i = 1;
  //int divisor;
  int divisor; //STANDARD mode
  //Mode = ModeDisplayButton();
  scale.tare();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.write("********************");
  lcd.setCursor(0,3);
  lcd.write("********************");
  lcd.setCursor(0,1);
  lcd.write("*");
  lcd.setCursor(0,2);
  lcd.write("*");
  lcd.setCursor(19,1);
  lcd.write("*");
  lcd.setCursor(19,2);
  lcd.write("*");
  lcd.setCursor(6, 1);
  lcd.write("FEEDING");
  lcd.setCursor(7, 2);
  lcd.write("TIME!");
  float initial_reading = scale.get_units(10);
  if (Mode == 4)
    divisor = 4; //INCREMENT mode
  else 
    divisor = 1;//STANDARD mode
  while (i <= divisor)
  {
    while ((scale.get_units(5) - initial_reading) < (food_val / divisor)) //divisor is 4 for INCREMENT, breaking up weight into 4 increments
    {
      ServoDispenser();
      if (millis() - time_sec >= 1000) //to keep timer on track
      time_sec += 1000;
    }
    i++;
    DispenseDCMotor();
    delay(2000);
    time_sec = time_sec + 2000; //match delay
    scale.tare(); //added this because load cell broke and is no longer linear, removing weight does not reduce scale reading
    initial_reading = scale.get_units(10);
    display_reset = true;
    
  }
}

void DispenseDCMotor()
{
  analogWrite(DCmotorPin, 240);
  int delay_time = 5050; //5050 jjuapproprriate time for food to be dispensed and hole to return to original location
  delay(delay_time);
  time_sec = time_sec + delay_time; 
  analogWrite(DCmotorPin, 0); //turn motor off
  if (millis() - time_sec >= 1000) //to keep timer on track
      time_sec += 1000;
}

int ModeDisplayButton()
{
  mode_toggle = !mode_toggle;
  if (mode_toggle == false)
  {
    //Interval mode
    Mode = 4;
    //return Mode;
  }
  else
  {
    //standard mode
    Mode = 1;
  } 
  //return Mode;
}


boolean ManualFeedButton()
{
  Manual_Feed = !Manual_Feed;
  return Manual_Feed; //True = feeding with button  
}


void WaterTemp()
{
  Wire.beginTransmission(temp_address);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(temp_address, 1);
  //if (Wire.available() == 0)
  byte celsius;
  if (millis() - time_temp >= 1750)
  {
    time_temp += 1750;
    celsius = Wire.read();
    water_temp = round(celsius * 9.0 / 5.0 + 32.0); // c to F conversion
    if (water_temp >= 100) //just in case of overflow
      lcd.setCursor(15,0);
    else
    {
      lcd.setCursor(15, 0);
      lcd.print(" ");
    } 
    lcd.print(water_temp);
  }
}

void FeedingTimer() //time since last feeding
{
  if (millis() - time_sec >= 1000)
  {
    time_sec += 1000;
    sec++;
  }

  if (sec == 60)
  {
    minute++;
    sec = 0;
  }
  if (minute == 60)
  {
    minute = 0;
    hour++;
  }
  lcd.setCursor(7, 0);
  if (hour > 0)
    lcd.print(hour);
  else
    lcd.print("0");
  lcd.print(":");
  if (minute >= 10)
    lcd.print(minute);
  else
  {
    lcd.print("0");
    lcd.print(minute);
  }
  lcd.print(":");

  if (sec >= 10)
    lcd.print(sec);
  else
  {
    lcd.print("0");
    lcd.print(sec);
  }
  return hour;
}

void ServoDispenser() {
  //140 degrees position is over hole
  //Delay() is okay during feeding, all LCD and other functions put on hold intentionally
  for (int i = Hole_closed; i >= Hole_open; i--) {
    servoDisp.write(i);
    delay(5);
  }
  delay(150);
  time_sec = time_sec + 150; // needs to match delay
  for (int i = Hole_open; i <= Hole_closed; i++) {
    // Statements
    servoDisp.write(i);
    delay(5);
  }
  delay(250);
  time_sec = time_sec + 250; //needs to match delay
  if (millis() - time_sec >= 1000) //to keep timer on track
      time_sec += 1000;
}


int FoodAmountDial() {
  int amount_val = analogRead(dialPotPin);
  int amountLvl = map(amount_val, 1223,0, 4, 11); //had to over-extend for mechanical reasons
  constrain(amountLvl, 5, 10);
  float food_val;
  String lcd_val = "**";
  if (amountLvl > 9)
  {
    food_val = 9.0;
    lcd_val = "10";
  }
  else if (amountLvl > 8)
  {
    food_val = 8.0;
    lcd_val = "9";
  }
  else if (amountLvl > 7)
  {  
    food_val = 7.0;
    lcd_val = "8";
  }
  else if (amountLvl > 6)
  {  
    food_val = 5.0;
    lcd_val = "7";
  }
  else if (amountLvl > 5)
  {
    food_val = 3.0;
    lcd_val = "6";
  }
  else
  {
    food_val = 2.0;
    lcd_val = "5";
  }
  if (lcd_val != "10" || lcd_val != "11")
  {
    lcd.setCursor(11,4);
    lcd.print(" ");
  }
  lcd.setCursor(10, 3);
  lcd.print(lcd_val);
  return food_val;
}

void OriginalDisplay()
{
  lcd.backlight();
  lcd.begin(20, 4);
  lcd.setCursor(0, 0);
  lcd.print("Timer: ");
  lcd.setCursor(18, 0);
  lcd.print("oF");
  //lcd.print(char(0));
  //lcd.write(byte(1));
  lcd.setCursor(0, 2);
  lcd.print("Mode:");
  lcd.setCursor(0, 3);
  lcd.print("Food Lvl:");
  lcd.setCursor(0, 1);
  lcd.write("Weight: ");
  lcd.setCursor(18,3);
  lcd.write("*");
}

