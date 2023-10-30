#include "gpio.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "lwip/opt.h"
#include "mqtt_client.h"
#include "portmacro.h"
#include <stdint.h>
#include <stdio.h>

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
  uint32_t gpio_num = (uint32_t)arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void *arg) {
  uint32_t io_num;
  QueueHandle_t output = (QueueHandle_t)arg;
  for (;;) {
    if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
      int val = gpio_get_level(io_num);
      if (val) {
        xQueueSend(output, (void *)&val, (TickType_t)100);
      }
    }
  }
}

void gpio_setup(QueueHandle_t queue) {
  // zero-initialize the config structure.
  gpio_config_t io_conf = {};

  // interrupt of rising edge
  io_conf.intr_type = GPIO_INTR_POSEDGE;
  // bit mask of the pins, use GPIO13 here
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
  // set as input mode
  io_conf.mode = GPIO_MODE_INPUT;
  // enable pull-up mode
  io_conf.pull_up_en = 1;
  gpio_config(&io_conf);

  // change gpio interrupt type for one pin
  gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

  // install gpio isr service
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

  // create a queue to handle gpio event from isr
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

  // start gpio task
  xTaskCreate(gpio_task, "gpio_task", 2048, (void *)queue, 10, NULL);

  // hook isr handler for specific gpio pin
  gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler,
                       (void *)GPIO_INPUT_IO_0);
}
