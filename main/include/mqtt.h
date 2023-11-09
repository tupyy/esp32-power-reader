#include "esp_err.h"
#include "esp_event.h"
#include "esp_system.h"
#include "portmacro.h"
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
#define DATA_TEMPLATE                                                          \
  "{"                                                                          \
  "\"value\":\"%d\","                                                          \
  "\"period\":\"%lld\""                                                        \
  "}"

typedef struct {
  char topic[20];
  char payload[100];
} mqtt_message;

BaseType_t publish(mqtt_message msg, bool wait);
int mqtt_start();
