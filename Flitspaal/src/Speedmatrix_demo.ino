/*
    Sneaky Fitness Speedtrap
*/

/*
    HUB 75  - ESP32 pin / comment
    R1	2	Red Data (columns 1-16)
    G1	15	Green Data (columns 1-16) 
    B1	4	Blue Data (columns 1-16)
    GND	GND	Ground
    R2	16/RX2	Red Data (columns 17-32)
    G2	27	Green Data (columns 17-32)  
    B2	17/TX2	Blue Data (columns 17-32)
    E	12	Demux Input E for 64x64 panels  
    A	5	Demux Input A0
    B	18	Demux Input A1
    C	19	Demux Input A2
    D	21	Demux Input E1, E3 (32x32 panels only)    
    CLK	22	LED Drivers' Clock
    STB	26	LED Drivers' Latch   
    OE	25	LED Drivers' Output Enable
    GND	GND	Ground
*/

#include "main.h"

// #includes library's
#include <SmartMatrix3.h>
#include "colorwheel.c"
#include "gimpbitmap.h"

//SPEED SENSOR
#define SPEED_PIN 32 //FOUT default pin 4
#define AVERAGE 7    //number of samples for averaging

// these variables need to be assigned volatile, since they can be updated from both processorcores independenlty.
volatile unsigned int doppler_div = 44;
volatile unsigned int samples[AVERAGE];
volatile unsigned int x;
volatile unsigned int speed;
volatile unsigned int kmh;
volatile unsigned int Freq;
volatile unsigned int Ttime;
volatile unsigned int lastSpeed;

//LED MATRIX
#define SMARTMATRIX_HUB75_64ROW_MOD16SCAN 10
#define COLOR_DEPTH 24 // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
#define ROW1 32
#define ROW2 0
#define ROW3 48
#define ROW4 16

const uint8_t kMatrixWidth = 64;                              // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 64;                             // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 48;                             // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 4;                             // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate //NOT USED BY ESP32
const uint8_t kPanelType = SMARTMATRIX_HUB75_64ROW_MOD16SCAN; // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);    // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

const int defaultBrightness = (100*255)/100;        // full (100%) brightness
// const int defaultBrightness = (50 * 255) / 100; // dim: 15% brightness
const int defaultScrollOffset = 6;
const rgb24 defaultBackgroundColor = {0, 0, 0};

//possibility to draw bitmaps on the screen max 15
void drawBitmap(int16_t x, int16_t y, const gimp32x32bitmap *bitmap)
{
  for (unsigned int i = 0; i < bitmap->height; i++)
  {
    for (unsigned int j = 0; j < bitmap->width; j++)
    {
      rgb24 pixel = {bitmap->pixel_data[(i * bitmap->width + j) * 3 + 0],
                     bitmap->pixel_data[(i * bitmap->width + j) * 3 + 1],
                     bitmap->pixel_data[(i * bitmap->width + j) * 3 + 2]};

      backgroundLayer.drawPixel(x + j, y + i, pixel);
    }
  }
}

// the setup() method runs once, when the sketch starts
void setup()
{
  Serial.begin(115200);

  //Init the pin for the CDM324 Radar sensor
  pinMode(SPEED_PIN, INPUT);

  //Create time and print it to terminal
  Serial.print("Pixel draw latency in us: ");
  unsigned long start_timer = micros();
  unsigned long delta_timer = micros() - start_timer;
  Serial.println(delta_timer);
  Serial.print("Display update latency in us: ");
  start_timer = micros();
  delta_timer = micros() - start_timer;
  Serial.println(delta_timer);

  //add matrix layers for background scrolling and index
  matrix.addLayer(&backgroundLayer);
  matrix.addLayer(&scrollingLayer);
  matrix.addLayer(&indexedLayer);
  matrix.begin();
  matrix.setBrightness(20);

  scrollingLayer.setOffsetFromTop(0); //defaultScrollOffset == 6
  backgroundLayer.enableColorCorrection(true);

  yield();
  xTaskCreatePinnedToCore(speedLoop, "sensorLoop", 4096, NULL, 1, NULL, BACKGROUND_RUNNING_CORE); // TODO, create task that handles the mainloop here.
  pinMode(LED_BUILTIN, OUTPUT);
  delay(2000);
}

void loop()
{
  // Serial.println(speed);
  const int leftEdgeOffset = 0;
  // clear screen
  matrix.setBrightness(defaultBrightness);
  backgroundLayer.fillScreen(defaultBackgroundColor);
  backgroundLayer.swapBuffers();

  // const int testBitmapWidth = 15;
  // const int testBitmapHeight = 15;
  // uint8_t testBitmap[] = {
  //     _______X, ________,
  //     ______XX, X_______,
  //     ______XX, X_______,
  //     ______XX, X_______,
  //     _____XXX, XX______,
  //     XXXXXXXX, XXXXXXX_,
  //     _XXXXXXX, XXXXXX__,
  //     __XXXXXX, XXXXX___,
  //     ___XXXXX, XXXX____,
  //     ____XXXX, XXX_____,
  //     ___XXXXX, XXXX____,
  //     ___XXXX_, XXXX____,
  //     __XXXX__, _XXXX___,
  //     __XX____, ___XX___,
  //     __X_____, ____X___,
  // };

  backgroundLayer.fillScreen({0, 0, 0});
  backgroundLayer.swapBuffers();
  backgroundLayer.setFont(font8x13);
  //REGEL 1
  // backgroundLayer.drawString(9, ROW1, {255, 83, 49}, "SNEAKY");
  //REGEL 2
  // backgroundLayer.drawString(0, ROW2, {4, 217, 178}, "HOE HARD");
  backgroundLayer.drawString(0, ROW2, {4, 217, 178}, "HOW FAST");
  //REGEL 3
  // backgroundLayer.drawString(0, ROW3, {238, 5, 242}, "REN JIJ?");
  backgroundLayer.drawString(4, ROW3, {238, 5, 242}, "CAN YOU");
  //REGEL 4
  backgroundLayer.drawString(8, ROW4, {255, 12, 255}, " RUN?");

  // backgroundLayer.drawMonoBitmap(24,49,
  //                         testBitmapWidth, testBitmapHeight, {(uint8_t)random(256), (uint8_t)random(256), 0}, testBitmap);
  backgroundLayer.swapBuffers();
  delay(10000);
}

// the loop() method runs over and over again,
// as long as the board has power
void loop_unused()
{
  int i, j;
  unsigned long currentMillis;

  // clear screen
  backgroundLayer.fillScreen(defaultBackgroundColor);
  backgroundLayer.swapBuffers();
}

//SPEED SENSOR LOOP
void speedLoop(void *pvParameters)
{
  const long setupTimeout{300000L};
  while (true)
  {
    // noInterrupts();
    // unsigned long start_timer = micros();
    // Wait for rising pulse. If it does not arrive, then try again
    if (pulseIn(SPEED_PIN, HIGH, setupTimeout) == setupTimeout)
    {
      vTaskDelay(1); // Allow the processor to do other stuff before continuing, preventing WDT timeout
      continue;
    }
    unsigned int pulse_length = 0;
    for (x = 0; x < AVERAGE; x++)
    {
      pulse_length = pulseIn(SPEED_PIN, HIGH, 300000L); // timeout of 5ms
      pulse_length += pulseIn(SPEED_PIN, LOW, 300000L);
      samples[x] = pulse_length;
    }
    // unsigned long delta_timer = micros() - start_timer;
    // Serial.print("input time in micros ");
    // Serial.println(delta_timer);
    vTaskDelay(1);
    // yield();
    // interrupts();

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
    if (samples_ok)
    {
      unsigned int Ttime = nbPulsesTime / AVERAGE;
      if (Ttime == 0)
        continue; // Don't run the stuff below, because that would be dividing by 0!
      unsigned int Freq = 1000000L / Ttime;

      //serial print lines uncomment when serial needed 
      // Serial.print(Ttime);
      // Serial.print("\r\n");
      // Serial.print(Freq);
      // Serial.print("Hz : ");
      // Serial.print(Freq / doppler_div);
      // Serial.print((Freq / doppler_div)*0.62137119223733);
      kmh = Freq / doppler_div; 
      speed = kmh * 0.62137119223733; // speed in MPH 
      // speed = Freq / doppler_div; // use this one for KM/h
      // Serial.print("km/h\r\n");
      // Serial.print("mph\r\n");

      if (speed > 2 && speed < 22  )
      // if (speed > 2 && speed < 30  )//km/h
      {
        backgroundLayer.fillScreen({0, 0, 0});
        backgroundLayer.swapBuffers();
        String speedString = (String)speed;
        //REGEL 1
        // backgroundLayer.drawString(10, ROW1, {4, 217, 178}, "LEKKER");
        backgroundLayer.drawString(15, ROW1, {4, 217, 178}, "GOOD");
        //REGEL 2
        backgroundLayer.drawString(27, ROW2, {0xff, 0xff, 0xff}, speedString.c_str());
        //REGEL 3
        // backgroundLayer.drawString(16, ROW3, {0xff, 0xff, 0xff}, "KM/H");
        backgroundLayer.drawString(19, ROW3, {0xff, 0xff, 0xff}, "MPH");
        //REGEL 4
        // backgroundLayer.drawString(10, ROW4, {4, 217, 178}, "BEZIG!");
        backgroundLayer.drawString(16, ROW4, {4, 217, 178}, "JOB!");
        backgroundLayer.swapBuffers();
      }
      else if (speed >= 22)//mph
      // else if (speed >= 31)//km/h
      {
        backgroundLayer.fillScreen({0, 0, 0});
        backgroundLayer.swapBuffers();
        String speedString = (String)speed;
        //REGEL 1
        backgroundLayer.drawString(0, ROW1, {255, 83, 49}, "COMPUTER");
        //REGEL 2
        backgroundLayer.drawString(16, ROW2, {0xff, 0xff, 0xff}, "SAYS");
        //REGEL 3
        backgroundLayer.drawString(22, ROW3, {0xff, 0xff, 0xff}, "NO");
        //REGEL 4
        backgroundLayer.drawString(22, ROW4, {255, 12, 255}, ":(");
        backgroundLayer.swapBuffers();
      }
    }

    speed++;
    vTaskDelay(1); // One tick delay to allow other processes that might be running on this core to do their thing
    // vTaskDelayUntil( &xLastWakeTime, xDelay );
  }
}