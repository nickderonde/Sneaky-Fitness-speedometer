#ifndef CONFIG_H
#define CONFIG_H

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"

#endif

#include "arduino.h"
#if CONFIG_FREERTOS_UNICORE // which core should we run
#define ARDUINO_RUNNING_CORE 0
#define BACKGROUND_RUNNING_CORE 1
#else
#define ARDUINO_RUNNING_CORE 1
#define BACKGROUND_RUNNING_CORE 0
#endif
void speedLoop( void * pvParameters);
#endif