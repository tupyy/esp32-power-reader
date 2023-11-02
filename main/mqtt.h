#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
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

#define MQTT_TAG "mqtt"
#define DATA_TEMPLATE "{\"value\":\"%d\",\"period\":\"%lld\"}"

int mqtt_connect(const char *host);
int mqtt_publish(const char *topic, const char *data);

typedef struct {
  char topic[20];
  char payload[100];
} mqtt_message;
