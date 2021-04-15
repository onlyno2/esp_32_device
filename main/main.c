#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "nvs_flash.h"

#include "wifi.h"

#define MQTT_HOST CONFIG_MQTT_HOST
#define MQTT_PORT 1883
#define MQTT_USERNAME CONFIG_MQTT_USERNAME
#define MQTT_CLIENTID CONFIG_MQTT_CLIENTID
#define MQTT_PASSWORD CONFIG_MQTT_PASSWORD

#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "esp_log.h"

#include "event_string_formats.h"

static const char *SENSOR_TAG = "SENSOR: ";

/** Cài đặt thư viện dht và định nghĩa các cấu hình */
#include "dht.h"
static dht_sensor_type_t dht_type = DHT_TYPE_DHT11;
static const gpio_num_t dht_gpio = 27;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;
static const char *MQTT_TAG = "Mqtt_driver_log: ";
esp_mqtt_client_handle_t mqtt_client = NULL;

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(MQTT_TAG, "sent publish successful, msg_id=%d", msg_id);

        /** TODO: Module hóa ở đây **/
        msg_id = esp_mqtt_client_subscribe(client, "v1/cmd/cmd_id", 0);
        ESP_LOGI(MQTT_TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        /** TODO: Thêm các hành động khi nhận lệnh điều khiển **/
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = MQTT_HOST,
        .port = MQTT_PORT,
        .username = MQTT_USERNAME,
        .client_id = MQTT_CLIENTID,
        .password = MQTT_PASSWORD};

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

void dht_read(void *pvParameters)
{
    float t = 0;
    float h = 0;
    char *payload = NULL;

    while (1)
    {
        if (dht_read_float_data(dht_type, dht_gpio, &h, &t) == ESP_OK)
        {
            ESP_LOGI(SENSOR_TAG, "DHT11 Temperature: %.2f, Humidity: %.2f", t, h);
            payload = (char*)malloc(60);
            sprintf(payload, DHT_REPORT_EVENT_PAYLOAD, t, h);
            esp_mqtt_client_publish(mqtt_client, "v1/evt/" DHT_REPORT_EVENT_ID, payload, strlen(payload), 0, false);
            free(payload);
        }
        else
        {
            ESP_LOGI(SENSOR_TAG, "DHT11 cannot read data");
        }

        vTaskDelay(50000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    initialize_wifi();
    mqtt_app_start();

    xTaskCreate(dht_read, "dht_read", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}
