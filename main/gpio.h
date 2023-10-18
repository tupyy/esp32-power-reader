#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPIO_INPUT_IO_0 13
#define GPIO_INPUT_PIN_SEL 1ULL << GPIO_INPUT_IO_0

#define ESP_INTR_FLAG_DEFAULT 0

void gpio(void);
