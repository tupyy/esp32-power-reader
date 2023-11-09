#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/projdefs.h"
#include "nvs_flash.h"
#include "portmacro.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_intsup.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "esp_log.h"
#include "mqtt.h"
#include "mqtt_client.h"
#include "wifi.h"
#include "sntp.h"

#include "gpio.h"
#include "sdkconfig.h"

void read_gpio_task(void *arg) {
  ESP_LOGI("app", "start read gpio task");

  QueueHandle_t queue = (QueueHandle_t)arg;
  uint32_t io_num;
  int ticks = 0;

  int64_t start_time = esp_timer_get_time();

  for (;;) {
    while (xQueueReceive(queue, &io_num, (TickType_t)0)) {
      if ((ticks + 1) < INT_MAX) {
        ticks++;
      } else {
        ticks = 0;
      }
    }

    if (ticks == 0) {
      continue;
    }

    int64_t now = esp_timer_get_time();
    int64_t period = (now - start_time) / 1000000;

    mqtt_message msg = {
        .topic = CONFIG_MQTT_DATA_TOPIC,
    };
    sprintf(msg.payload, DATA_TEMPLATE, ticks, period);
    ESP_LOGD("app", "payload:%s, topic:%s\n", msg.payload, msg.topic);

    // sent to publish
    if (publish(msg, false)) {
      ticks = 0;
      start_time = now;
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    } else {
      ESP_LOGE("app", "failed to write to mqtt_queue");
    }
  }
}

void app_main(void) {
  ESP_LOGI("app", "[ReadWatt] Startup..");
  ESP_LOGI("app", "[ReadWatt] Free memory: %" PRIu32 " bytes",
           esp_get_free_heap_size());
  ESP_LOGI("app", "[ReadWatt] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_DEBUG);
  esp_log_level_set("app", ESP_LOG_VERBOSE);
  esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
  esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
  esp_log_level_set("transport", ESP_LOG_VERBOSE);
  esp_log_level_set("outbox", ESP_LOG_VERBOSE);

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  /* This helper function configures Wi-Fi or Ethernet, as selected in
   * menuconfig. Read "Establishing Wi-Fi or Ethernet Connection" section
   * in examples/protocols/README.md for more information about this
   * function.
   */
  ESP_ERROR_CHECK(wifi_connect());

  ESP_LOGI("app", "BROKER_URL:%s, DATA_TOPIC:%s\n", CONFIG_BROKER_URL,
           CONFIG_MQTT_DATA_TOPIC);

  sync_time();

    mqtt_start();

  //  QueueHandle_t queue = xQueueCreate(10, sizeof(uint32_t));

  /* gpio_setup(queue); */

  /* xTaskCreate(read_gpio_task, "read_gpio_task", 2048, (void *)queue, 10,
   * NULL); */

  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
