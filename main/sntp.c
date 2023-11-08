#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "lwip/ip_addr.h"
#include "nvs_flash.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *SNTP_TAG = "sntp";

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

static void obtain_time(void);

void time_sync_notification_cb(struct timeval *tv) {
  ESP_LOGI(SNTP_TAG, "Notification of a time synchronization event");
}

void sync_time(void) {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  if (timeinfo.tm_year < (2016 - 1900)) {
    ESP_LOGI(SNTP_TAG, "Time is not set yet. Get time over NTP.");
    obtain_time();
    time(&now);
  }

  char strftime_buf[64];

  setenv("TZ", "CET-1", 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(SNTP_TAG, "The current date/time in Paris is: %s", strftime_buf);

  if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
    struct timeval outdelta;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
      adjtime(NULL, &outdelta);
      ESP_LOGI(
          SNTP_TAG,
          "Waiting for adjusting time ... outdelta = %jd sec: %li ms: %li us",
          (intmax_t)outdelta.tv_sec, outdelta.tv_usec / 1000,
          outdelta.tv_usec % 1000);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
  }
}

static void print_servers(void) {
  ESP_LOGI(SNTP_TAG, "List of configured NTP servers:");

  for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i) {
    if (esp_sntp_getservername(i)) {
      ESP_LOGI(SNTP_TAG, "server %d: %s", i, esp_sntp_getservername(i));
    } else {
      // we have either IPv4 or IPv6 address, let's print it
      char buff[INET6_ADDRSTRLEN];
      ip_addr_t const *ip = esp_sntp_getserver(i);
      if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
        ESP_LOGI(SNTP_TAG, "server %d: %s", i, buff);
    }
  }
}

static void obtain_time(void) {
  ESP_LOGI(SNTP_TAG, "Initializing and starting SNTP");

  esp_sntp_config_t config =
      ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
  config.sync_cb = time_sync_notification_cb;
  config.smooth_sync = true;

  esp_netif_sntp_init(&config);

  print_servers();

  // wait for time to be set
  time_t now = 0;
  struct tm timeinfo = {0};
  int retry = 0;
  const int retry_count = 15;
  while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) ==
             ESP_ERR_TIMEOUT &&
         ++retry < retry_count) {
    ESP_LOGI(SNTP_TAG, "Waiting for system time to be set... (%d/%d)", retry,
             retry_count);
  }
  time(&now);
  localtime_r(&now, &timeinfo);
}
