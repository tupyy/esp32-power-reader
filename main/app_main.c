#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/projdefs.h"
#include "nvs_flash.h"
#include "portmacro.h"
#include "protocol_examples_common.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "gpio.h"
#include "mqtt.h"
#include "sdkconfig.h"

static SemaphoreHandle_t semaphore;
static int ticks = 0;
static int64_t timer = 0;

void read_gpio_task(void *arg) {
  ESP_LOGI("app", "start read gpio task");
  QueueHandle_t queue = (QueueHandle_t)arg;
  uint32_t io_num;

  for (;;) {
    if (xSemaphoreTake(semaphore, (TickType_t)10)) {
      while (xQueueReceive(queue, &io_num, 0)) { // deplee the queue
        ticks++;
      }
      xSemaphoreGive(semaphore);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void publish_task(void *arg) {
  ESP_LOGI("app", "start publish task");
  const char *template = "{\"value\":\"%d\",\"period\":\"%lld\"}";
  char data[strlen(template) + 20];

  for (;;) {
    if (xSemaphoreTake(semaphore, (TickType_t)10)) {
      if (ticks == 0) {
        xSemaphoreGive(semaphore);
        continue;
      }

      int64_t new_time = esp_timer_get_time();
      if (timer > 0) {
        sprintf(data, template, ticks, (new_time - timer) / 1000000);
      } else {
        sprintf(data, template, ticks, (int64_t)0);
      }

      ESP_LOGI("data", "written %s to topic\n", data);

      ticks = 0;
      timer = new_time;

      // release the semaphore
      xSemaphoreGive(semaphore);

      if (mqtt_publish("topic/qos0", data) != 0) {
        ESP_LOGE("mqtt", "unable to publish to topic \"topic/qos0\"");
      }
      // wait only if we got the semaphore
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    } else {
      ESP_LOGE("mqtt", "cannot get the semaphore");
    }
  }
}

void app_main(void) {
  ESP_LOGI("app", "[ReadWatt] Startup..");
  ESP_LOGI("app", "[ReadWatt] Free memory: %" PRIu32 " bytes",
           esp_get_free_heap_size());
  ESP_LOGI("app", "[ReadWatt] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
  esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
  esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
  esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
  esp_log_level_set("transport", ESP_LOG_VERBOSE);
  esp_log_level_set("outbox", ESP_LOG_VERBOSE);

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  /* This helper function configures Wi-Fi or Ethernet, as selected in
   * menuconfig. Read "Establishing Wi-Fi or Ethernet Connection" section in
   * examples/protocols/README.md for more information about this function.
   */
  ESP_ERROR_CHECK(example_connect());

  semaphore = xSemaphoreCreateBinary();
  if (semaphore == NULL) {
    ESP_LOGE("app", "unable to create counter semaphore");
    return;
  }

  if (mqtt_connect(CONFIG_BROKER_URL)) {
    ESP_LOGE("app", "cannot connect to nats");
    return;
  }

  QueueHandle_t queue = xQueueCreate(10, sizeof(uint32_t));
  gpio_setup(queue);

  xTaskCreate(read_gpio_task, "read_gpio_task", 2048, (void *)queue, 10, NULL);
  xTaskCreate(publish_task, "publish_task", 2048, NULL, 10, NULL);

  xSemaphoreGive(semaphore);
  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
