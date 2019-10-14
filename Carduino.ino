/*

 CARduino
 
 This sketch powers the CARduino in my WRX.
 Currently, it only has telemetry features.
 More features coming soon (hopefully).
 
 The circuit:
 board: Arduino Uno R3
 A0: accelerometer x-axis
 A1: accelerometer y-axis
 D2: [interrupt] Calibrate button
 D6: LCD serial Rx (unused)
 D7: LCD serial Tx
 
 Accelerometer is mounted with:
 +X: Vehicle passenger side
 +Y: Vehicle front
 
 created Nov/Dec 2012
 by Sean Welton
 
 This code is CC licensed BY-NC-SA
 http://creativecommons.org/licenses/by-nc-sa/3.0/
 DO WITH IT AS YOU PLEASE, with attribution
 
 */
 
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <TimedAction.h>   // Helper library to time actions http://arduino.cc/playground/Code/TimedAction

String VERSION = "1.2.0";

/* Declare global variables ------------------------- */

// Declare constants
const int xpin = A0;                           // x-axis pin
const int ypin = A1;                           // y-axis pin
const int led = 13;                            // LED pin
const double ADC_V_per_count = 5.0 / 1024.0;   // ADC range (0-5 Vdc, 1024 counts)

// X calibration
const double x_g_per_Volt = 1.0 / 0.336;       // Accelerometer sensitivity (300 mV/g)
volatile double x_q_Volts = 1.62;              // Quiescent voltage of the accelerometer
// Y calibration
const double y_g_per_Volt = 1.0 / 0.336;       // Accelerometer sensitivity (300 mV/g)
volatile double y_q_Volts = 1.62;              // Quiescent voltage of the accelerometer

// Other variables
static double score;                           // Overall accumulated score
static double scoreAve;                        // Average scoring rate (AKA points/minute)
static int recordScoreAve;                     // Highest ever (persistent) score/min
static double scoreRate = 5.0;                 // Rate at which the user accumulates points
boolean clearScreen = false;                   // Bool - Whether we need to clear the LCD screen

// LCD Line 1/2 Display Modes
int l1_mode = 0;
/* 
 0 = Lateral G

 */
int l2_mode = 0;
/*
 0 = G Score
 1 = G Score / minute
 2 = Max lateral G
 3 = Max longitudinal G
 4 = Record score/min
 */

// EEPROM locations 
int xCalAddr = 0;    // X axis Q-Voltage setting
int yCalAddr = 1;    // Y axis Q-Voltage setting
int recordA  = 2;    // Record score divisor
int recordB  = 3;    // Record score remainder

/* End global variables -------------------------- */

// Declare the LCD's serial port
SoftwareSerial lcd(6,7);

// Declare the TimedAction which will switch LCD line2 every 5 seconds
TimedAction l2_modeChange = TimedAction(5000,changeL2mode);


/* Begin functions ============================================================== */

double read_x_g()
{
  /*
    Read the X value from the accelerometer 10 times and average the results
    This helps to smooth the readings  
   */

  int x_count = 0;                       // X-axis ADC counts
  double x_Volts = 0.0;                  // X-axis absolute voltage
  double x_Volts_delta = 0.0;            // X-axis relative voltage
  double x_g = 0.0;                      // X-axis array of 3 G-force samples
  double x_g_force;                      // Averaged X-axis G-force
  double x_g_force_r;                    // X G rounded to nearest tenth
  double sum_x;                          // Sum of X G samples

  for (int i=0; i < 10; i++){
    // Get the x ADC value, then convert to volts and g-force
    x_count = analogRead(xpin);
    x_Volts = x_count * ADC_V_per_count;
    x_Volts_delta = x_Volts - x_q_Volts;
    x_g = x_Volts_delta * x_g_per_Volt;
    // Add this g value to the running sum total
    sum_x += x_g;
    delay(5);        // This delay sets the spacing of the averaged samples
  }

  // Now divide to get the average
  x_g_force = sum_x / 10.0;
  // Round the result to the nearest tenth
  x_g_force_r = round(10.0 * x_g_force)/10.0;

  return x_g_force_r;

}

//----------------------------------------------------------------------------------

double read_y_g()
{
  /*
    Read the Y value from the accelerometer 10 times and average the results
    This helps to smooth the readings  
   */

  int y_count = 0;                       // y-axis ADC counts
  double y_Volts = 0.0;                  // y-axis absolute voltage
  double y_Volts_delta = 0.0;            // y-axis relative voltage
  double y_g = 0.0;                      // y-axis array of 3 G-force samples
  double y_g_force;                      // Averaged y-axis G-force
  double y_g_force_r;                    // Y G rounded to nearest tenth
  double sum_y;                          // Sum of Y G samples

  for (int i=0; i < 10; i++){
    // Get the y ADC value, then convert to volts and g-force
    y_count = analogRead(ypin);
    y_Volts = y_count * ADC_V_per_count;
    y_Volts_delta = y_Volts - y_q_Volts;
    y_g = y_Volts_delta * y_g_per_Volt;
    // Add this g value to the running sum total
    sum_y += y_g;
    delay(5);        // This delay sets the spacing of the averaged samples
  }

  // Now divide to get the average
  y_g_force = sum_y / 10.0;
  // Round the result to the nearest tenth
  y_g_force_r = round(10.0 * y_g_force)/10.0;

  return y_g_force_r;

}

//----------------------------------------------------------------------------------

void setBacklight(byte brightness)
{
  // Use this function to set the LCD's backlight brightness
  
  lcd.write(0x80);        // send the backlight command
  lcd.write(brightness);  // send the brightness value
}

//----------------------------------------------------------------------------------

void clearDisplay()
{
  // Use this function to clear the LCD display
  
  lcd.write(0xFE);  // send the special command
  lcd.write(0x01);  // send the clear screen command
}

//----------------------------------------------------------------------------------

void setLCDCursor(byte cursor_position)
{
  // Use this function to move the LCD cursor
  
  lcd.write(0xFE);              // send the special command
  lcd.write(0x80);              // send the set cursor command
  lcd.write(cursor_position);   // send the cursor position
}

//----------------------------------------------------------------------------------

void showTopScore(double topScore)
{
  // This function displays a modal notification of
  // a new highest lateral g reading
  
  // Print Line 1
  clearDisplay();
  setLCDCursor(0);
  lcd.print("New Top Score!");
  // Print Line 2
  setLCDCursor(16);
  printDouble(topScore,10);
  lcd.print(" Lateral G");
  // Now that we've printed modally, set clearScreen true to clear at next refresh
  clearScreen = true;

}

void showRecordScore(int recordScore)
{
  // This function displays a modal notification of
  // a new highest points/min
  
  // Print Line 1
  clearDisplay();
  setLCDCursor(0);
  lcd.print("New Record!");
  // Print Line 2
  setLCDCursor(16);
  lcd.print(recordScore);
  lcd.print(" pts/min");
  // Now that we've printed modally, set clearScreen true to clear at next refresh
  clearScreen = true;

}

//----------------------------------------------------------------------------------

/*void changeL1mode(){
  NOT USED YET
 }*/

//----------------------------------------------------------------------------------

void changeL2mode()
{
  // Change the Line 2 display mode
  // NOTICE WE'RE NOT USING 3 YET
  
  switch (l2_mode){
  case 0:
    l2_mode = 1;
    clearScreen = true;
    break;
  case 1:
    l2_mode = 2;
    clearScreen = true;
    break;
  case 2:
    l2_mode = 4;
    clearScreen = true;
    break;
  case 4:
    l2_mode = 0;
    clearScreen = true;
    break;
  default:
    l2_mode = 0;
    clearScreen = true;
  }
}

//----------------------------------------------------------------------------------

void animateStartup()
{
  // This function displays an animation at boot

  int textDelay = 300;     // Amount of delay between letters

  // Get the voltages for early debug/cal
  int x_count = analogRead(xpin);
  double x_Volts = x_count * ADC_V_per_count;
  int y_count = analogRead(ypin);
  double y_Volts = y_count * ADC_V_per_count;

  // line 1
  clearDisplay();
  setLCDCursor(4);
  lcd.print("C");
  delay(textDelay);
  lcd.print("A");
  delay(textDelay);
  lcd.print("R");
  delay(textDelay);
  lcd.print("d");
  delay(textDelay);
  lcd.print("u");
  delay(textDelay);
  lcd.print("i");
  delay(textDelay);
  lcd.print("n");
  delay(textDelay);
  lcd.print("o ");
  setLCDCursor(20);
  delay(textDelay);
  lcd.print("v. ");
  lcd.print(VERSION);

  /* // line 2
  setLCDCursor(16);
  lcd.print("X: ");
  lcd.print(x_Volts);
  lcd.print("  Y: ");
  lcd.print(y_Volts);*/

}

//----------------------------------------------------------------------------------

void calibrate()
{
  // This function is called whenever the calibrate button interrupt is triggered.

  int x_count = 0;                       // X-axis ADC counts
  double x_Volts = 0.0;                  // X-axis absolute voltage
  int y_count = 0;                       // Y-axis ADC counts
  double y_Volts = 0.0;                  // Y-axis absolute voltage

  // Get the x ADC value, then convert to volts
  x_count = analogRead(xpin);
  x_Volts = x_count * ADC_V_per_count;
  // Get the y ADC value, then convert to volts
  y_count = analogRead(ypin);
  y_Volts = y_count * ADC_V_per_count;

  // Now that we have the current x/y volts, set them as the q points
  x_q_Volts = x_Volts;
  y_q_Volts = y_Volts;
  
  // Now store these to EEPROM
  // It's important to note that we can only store 0-255 in EEPROM,
  // but we'll multiply our cal point by 100 to convert to integer
  int xCal = x_q_Volts * 100;
  int yCal = y_q_Volts * 100;
  EEPROM.write(xCalAddr, xCal);
  EEPROM.write(yCalAddr, yCal);

  // Also clear the record score
  writeRecord(0);
  recordScoreAve = 0;
  
}

//----------------------------------------------------------------------------------

void printDouble( double val, unsigned int precision)
{
  // prints val with number of decimal places determine by precision
  // NOTE: precision is 1 followed by the number of zeros for the desired number of decimial places
  // example: printDouble( 3.1415, 100); // prints 3.14 (two decimal places)
  
  // http://arduino.cc/forum/index.php/topic,44216.0.html

  lcd.print (int(val));   //prints the int part
  lcd.print(".");         // print the decimal point
  unsigned int frac;
  if(val >= 0)
    frac = (val - int(val)) * precision;
  else
    frac = (int(val)- val ) * precision;
  lcd.print(frac,DEC) ;
} 

int readRecord()
{
  // Reads the all-time record score/min from EEPROM
  
  int a = EEPROM.read(recordA);
  int b = EEPROM.read(recordB);
  
  int value = (a * 256) + b;

  return value;
}

void writeRecord(int val)
{
  // Writes a new all-time record score/min to EEPROM
  
  int a = val / 256;
  int b = val % 256;
  
  EEPROM.write(recordA, a);
  EEPROM.write(recordB, b);
}

/* End functions ================================================================*/

void setup()
{

  pinMode(led,OUTPUT);    // Set LED pin as output
  lcd.begin(9600);        // start lcd serial
  //Serial.begin(9600);    // Here mostly for debug
  setBacklight(75);      // Set backlight brightness
  animateStartup();       // Run the startup animation
  setLCDCursor(0);        // Go back to 0

  // Set up the interrupt for the calibrate button
  attachInterrupt(0, calibrate, RISING);
  
  x_q_Volts = EEPROM.read(xCalAddr) / 100.0;
  y_q_Volts = EEPROM.read(yCalAddr) / 100.0;
  
  // Read the current record score/min from EEPROM
  recordScoreAve = readRecord();

}

//----------------------------------------------------------------------------------

void loop()
{

  // Declare variables
  double composite = 0.0;                  // Composite XY G-vector
  double scaledComposite = 0.0;            // Composite scaled by score factor
  double displayX = 0.0;                   // Displayed (abs) value for lat. g
  double displayY = 0.0;                   // Displayed (abs) value for lon. g
  double displayComposite = 0.0;           // Displayed (abs) value for composite g
  static unsigned long displayScore;       // Displayed (rounded) score
  static int displayScoreAve;              // Displayed (rounded) score rate (points/min)
  static int lastDisplayScoreAve;          // Previous cycle's displayScoreAve
  static unsigned long millis_topScore;    // Millisecond value at which top lat. g achieved
  static unsigned long millis_recordScore; // Millisecond value at which record score achieved
  char x_dir;                              // X G sign (left/right lat. g)
  char y_dir;                              // Y G sign (fore/aft g)
  static double x_top_score;               // Highest Lat. G reading yet

  // Take the G-force readings
  double x = read_x_g();
  double y = read_y_g();

  if (x > 0){
    // If X is positive, Right G
    x_dir = '>'; 
  }
  else if (x < 0){
    // If X is negative, Left G
    x_dir = '<';
  }
  else {
    x_dir = ' ';
  }

  // Take absolute value of X for display purposes
  displayX = abs(x);
  displayY = abs(y);

  // Compute the composite G-force for scoring
  composite = sqrt(pow(x,2.0) + pow(y,2.0));
  displayComposite = abs(composite);

  // Keep score if G is over 0.2
  if (composite > 0.2){
    scaledComposite = composite * scoreRate;
    // Score += ( G_vector * 5 ) ^2
    score += pow(scaledComposite,2);
    displayScore = round(score);
  }

  // Compute the score rate
  scoreAve = score / (millis() / 60000.0);
  lastDisplayScoreAve = displayScoreAve;
  displayScoreAve = round(scoreAve);
  
  // If we've set a new record score/min, write it to EEPROM
  // But only if we've been driving for more than 5 minutes
  // And the score isn't still increasing
  // This helps to reduce unnecessary writes to EEPROM
  if ((displayScoreAve > recordScoreAve) 
       && (millis() > 300000)
       && (displayScoreAve < lastDisplayScoreAve)){
     recordScoreAve = displayScoreAve;
     writeRecord(recordScoreAve);
     millis_recordScore = millis();
     showRecordScore(recordScoreAve);
  }

     
  // Check whether we should rotate L2 display mode
  l2_modeChange.check();

  // If it's been more than 10 seconds since a high score,
  if ((millis() > millis_topScore + 10000)
   && (millis() > millis_recordScore + 10000)){
    
    // Print the 1st line on LCD
    if (clearScreen == true){
      clearDisplay();
      clearScreen = false;
    }
    
    setLCDCursor(0);
    
    lcd.print("Lat:");
    
    if (x_dir == '<'){
      lcd.print(x_dir);
    }
    else{
      lcd.print(" ");
    }
    
    printDouble(displayX,10);
    
    if (x_dir == '>'){
      lcd.print(x_dir);
    }
    else{
      lcd.print(" ");
    }

    lcd.print("  C:");
    
    printDouble(displayComposite,10);
    
    
    // Print the second line
    setLCDCursor(16);

    switch(l2_mode){
    case 0:
      // Accumulated score
      lcd.print("Score: ");
      lcd.print(displayScore);
      break;
    case 1:
      // Average score rate (points/min)
      lcd.print("Score/min: ");
      lcd.print(displayScoreAve);
      break;
    case 2:
      // Max lateral G
      lcd.print("Max lat. G: ");
      printDouble(x_top_score,10);
      break;
    case 4:
      // Record score/min
      lcd.print("Rec. p/m: ");
      lcd.print(recordScoreAve);
      break;
    default:
      // Unsupported data
      lcd.print("ERROR");
    }
  }

  // Now compare the Lateral G reading to current top score
  if (displayX > x_top_score){
    x_top_score = displayX;
    if (x_top_score > 0.3){
      millis_topScore = millis();
      showTopScore(x_top_score);
    }
  }

  delay(50);    // Delay 50 ms for LCD stability

}

// End file
