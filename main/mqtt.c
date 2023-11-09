#include "mqtt.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mqtt_client.h"
#include "portmacro.h"
#include "sdkconfig.h"

static esp_mqtt_client_handle_t client = NULL;
static QueueHandle_t publish_queue = NULL;
static TaskHandle_t public_task_handle = NULL;

// delay period for reconnect task
const TickType_t xDelay = 2000 / portTICK_PERIOD_MS;

int mqtt_connect();
void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                        int32_t event_id, void *event_data);

static void log_error_if_nonzero(const char *message, int error_code) {
  if (error_code != 0) {
    ESP_LOGE(MQTT_TAG, "Last error %s: 0x%x", message, error_code);
  }
}

BaseType_t publish(mqtt_message msg, bool wait) {
  TickType_t time_to_wait = (TickType_t)0;
  if (wait) {
    time_to_wait = portMAX_DELAY;
  }
  return xQueueSend(publish_queue, (void *)&msg, time_to_wait);
}

void publish_task(void *arg) {
  ESP_LOGI(MQTT_TAG, "start publish task");

  if (publish_queue == NULL) {
    publish_queue = xQueueCreate(10, sizeof(mqtt_message));
  }

  mqtt_message msg;
  for (;;) {
    xQueueReceive(publish_queue, (void *)&msg, portMAX_DELAY);
    ESP_LOGD(MQTT_TAG, "written %s to topic %s\n", msg.payload, msg.topic);

    if (esp_mqtt_client_publish(client, msg.topic, msg.payload, 0, 0, 0)) {
      ESP_LOGE(MQTT_TAG, "unable to publish to topic %s", msg.topic);
    }
  }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                        int32_t event_id, void *event_data) {
  ESP_LOGD(MQTT_TAG,
           "Event dispatched from event loop base=%s, event_id=%" PRIi32 "",
           base, event_id);
  esp_mqtt_event_handle_t event = event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(MQTT_TAG, "CONNECTED TO %s", CONFIG_BROKER_URL);
    if (public_task_handle == NULL) {
      xTaskCreate(publish_task, "publish_task", 2048, NULL, 10,
                  &public_task_handle);
    }
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
    if (public_task_handle != NULL) {
      vTaskDelete(public_task_handle);
      public_task_handle = NULL;
    }
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      log_error_if_nonzero("reported from esp-tls",
                           event->error_handle->esp_tls_last_esp_err);
      log_error_if_nonzero("reported from tls stack",
                           event->error_handle->esp_tls_stack_err);
      log_error_if_nonzero("captured as transport's socket errno",
                           event->error_handle->esp_transport_sock_errno);
      ESP_LOGI(MQTT_TAG, "Last errno string (%s)",
               strerror(event->error_handle->esp_transport_sock_errno));
    }
    break;
  default:
    ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
    break;
  }
}

// connect to mqtt broker and start the publish task
int mqtt_connect() {
  ESP_LOGI(MQTT_TAG, "CONNECTING TO %s", CONFIG_BROKER_URL);
  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = CONFIG_BROKER_URL,
  };

  client = esp_mqtt_client_init(&mqtt_cfg);

  return esp_mqtt_client_start(client);
}

int mqtt_start() {
  if (mqtt_connect() != ESP_OK) {
    return ESP_FAIL;
  }
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler,
                                 NULL);
  return ESP_OK;
}
