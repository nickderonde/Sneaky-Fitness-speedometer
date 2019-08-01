/*******************************************************************
    Using a 32 * 16 RGB Matrix to display for sneaky fitness
 *                                                                 *
    Written by Nick de Ronde
 *******************************************************************/


#include "arduino.h"
#include <Ticker.h>
#include <PxMatrix.h>

Ticker display_ticker;

// Pins for LED MATRIX
#define P_LAT 16
#define P_A 5
#define P_B 4
// #define P_C 15
//#define P_D 12
//#define P_E 0
#define P_OE 2

// pin number for FOUT
#define SPEED_PIN D8
// number of samples for averaging
#define AVERAGE 2

unsigned int doppler_div = 44;
unsigned int samples[AVERAGE];
unsigned int x;
unsigned int speed = 0;
unsigned int Freq = 0;
unsigned int Ttime = 0;
unsigned int lastSpeed = 0;


uint8_t display_draw_time = 0;

PxMATRIX display(32, 16, P_LAT, P_OE, P_A, P_B);

// Some standard colors
uint16_t myRED = display.color565(255, 0, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);

uint16_t myCOLORS[8] = {myRED, myGREEN, myBLUE, myWHITE, myYELLOW, myCYAN, myMAGENTA, myBLACK};

// ISR for display refresh
void display_updater()
{
  display.display(70);
  // display.display(display_draw_time);
}

void display_update_enable(bool is_enable)
{
  if (is_enable)
    display_ticker.attach(2.2, display_updater);
  else
    display_ticker.detach();
}

void speed_calculator()
{
  noInterrupts();
  display_update_enable(false);
  pulseIn(SPEED_PIN, HIGH);
  unsigned int pulse_length = 0;

  for (x = 0; x < AVERAGE; x++)
  {
    pulse_length = pulseIn(SPEED_PIN, HIGH, 700000L);
    pulse_length += pulseIn(SPEED_PIN, LOW, 700000L);

    samples[x] = pulse_length;
  }

  // yield();
  interrupts();

  // Check for consistency
  bool samples_ok = true;
  unsigned int nbPulsesTime = samples[0];
  for (x = 1; x < AVERAGE; x++)
  {
    nbPulsesTime += samples[x];
    if ((samples[x] > samples[0] * 2) || (samples[x] < samples[0] / 2))
    {
      samples_ok = false;
    }
  }

  display_update_enable(true);

  if (samples_ok)
  {
    Ttime = nbPulsesTime / AVERAGE;
    Freq = 1000000 / Ttime;
    speed = Freq / doppler_div;
    
    if (speed <= 5) {
      display.clearDisplay();
      display.setTextColor(myYELLOW);
      display.setCursor(0, 0);
      display.print("Too");
      display.setTextColor(myRED);
      display.setCursor(0, 8);
      display.print("Slow!");
    }

  }
  else if (speed == lastSpeed) { 
    display.setTextColor(myGREEN);
    display.setCursor(0, 0);
    display.print((String)speed);
    display.setTextColor(myMAGENTA);
    display.setCursor(0, 8);
    display.print("Km/h");
  }

  else
  {
    // Serial.print(Ttime);
    Serial.print(Freq);
    Serial.print("Hz : ");
    Serial.print(speed);
    Serial.print("km/h\r\n");

    display.clearDisplay();
    display.setTextColor(myGREEN);
    display.setCursor(0, 0);
    display.print((String)speed);
    display.setTextColor(myMAGENTA);
    display.setCursor(0, 8);
    display.print("Km/h");
  
    yield();
    // delay(500);
  }
  lastSpeed = speed;
  display.display(1000);
}

void setup()
{
  //Set baudrate to 115200
  Serial.begin(9600);

  //Init the pin for the CDM324 Radar sensor
  pinMode(SPEED_PIN, INPUT);

  // Define your display layout here, e.g. 1/2 step
  display.begin(2);

  // Define your scan pattern here {LINE, ZIGZAG, ZAGGIZ} (default is LINE)
  display.setScanPattern(ZAGGIZ);

  display.clearDisplay();
  Serial.print("Pixel draw latency in us: ");
  unsigned long start_timer = micros();
  display.drawPixel(1, 1, 0);
  unsigned long delta_timer = micros() - start_timer;
  Serial.println(delta_timer);

  Serial.print("Display update latency in us: ");
  start_timer = micros();
  display.display(70);
  delta_timer = micros() - start_timer;
  Serial.println(delta_timer);
  delay(2000);

  display_ticker.attach(0.002, display_updater);
  yield();
  
  display.clearDisplay();
  delay(500);
}


union single_double {
  uint8_t two[2];
  uint16_t one;
} this_single_double;

void loop()
{
  speed_calculator();
  
  // if (speed < 4)
  // {
  //   display.setTextColor(myCYAN);
  //   display.setCursor(0, 0);
  //   display.print("REN !");
  //   display.setTextColor(myMAGENTA);
  //   display.setCursor(0, 8);
  //   display.print("Harder");
  //   delay(1000);
  // }

  // else
  // {
  //   display.setTextColor(myCYAN);
  //   display.setCursor(0, 0);
  //   display.print((String)speed);
  //   display.setTextColor(myMAGENTA);
  //   display.setCursor(0, 8);
  //   display.print("Km/h");
  //   delay(1000);
  // }

  // display_update_enable(true);

  // yield();
  // delay(500);

  // display_update_enable(false);
}