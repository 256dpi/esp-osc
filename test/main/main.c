#include <stdlib.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <esp_osc.h>

#define WIFI_SSID ""
#define WIFI_PASS ""

#define OSC_ADDRESS ""
#define OSC_PORT 0

#define TAG "main"

esp_osc_client_t client;

static void sender() {
  for (;;) {
    // send remote message
    if (strlen(OSC_ADDRESS) > 0) {
      esp_osc_client_select(&client, OSC_ADDRESS, OSC_PORT);
      esp_osc_client_send(&client, "test", "ihfdsb", 42, (int64_t)84, 3.14f, 6.28, "foo", 3, "bar");
    }

    // send local message
    esp_osc_client_select(&client, "127.0.0.1", 9000);
    esp_osc_client_send(&client, "test", "ihfdsb", 42, (int64_t)84, 3.14f, 6.28, "foo", 3, "bar");

    // delay
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

static bool callback(const char *topic, const char *format, esp_osc_value_t *values) {
  // log message
  ESP_LOGI(TAG, "got message: %s (%s)", topic, format);
  for (size_t i = 0; i < strlen(format); i++) {
    switch (format[i]) {
      case 'i':
        ESP_LOGI(TAG, "==> i: %d", values[i].i);
        break;
      case 'h':
        ESP_LOGI(TAG, "==> h: %lld", values[i].h);
        break;
      case 'f':
        ESP_LOGI(TAG, "==> f: %f", values[i].f);
        break;
      case 'd':
        ESP_LOGI(TAG, "==> d: %f", values[i].d);
        break;
      case 's':
        ESP_LOGI(TAG, "==> s: %s", values[i].s);
        break;
      case 'b':
        ESP_LOGI(TAG, "==> b: %.*s (%d)", values[i].bl, values[i].b, values[i].bl);
        break;
    }
  }

  return true;
}

static void receiver() {
  for (;;) {
    // receive messages
    esp_osc_client_receive(&client, callback);
  }
}

static void handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
  if (base == WIFI_EVENT) {
    switch (id) {
      case SYSTEM_EVENT_STA_START:
        // connect to ap
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());

        break;

      case SYSTEM_EVENT_STA_GOT_IP:
        break;

      case SYSTEM_EVENT_STA_DISCONNECTED:
        // reconnect Wi-Fi
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());

        break;

      default:
        break;
    }
  }
}

void app_main() {
  // initialize NVS flash
  ESP_ERROR_CHECK(nvs_flash_init());

  // initialize networking
  ESP_ERROR_CHECK(esp_netif_init());

  // create default event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // enable Wi-Fi
  esp_netif_create_default_wifi_sta();

  // initialize Wi-Fi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // set Wi-Fi storage to ram
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  // set wifi mode
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

  // register event handlers
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler, NULL, NULL));

  // prepare Wi-Fi config
  wifi_config_t wifi_config = {.sta = {.ssid = WIFI_SSID, .password = WIFI_PASS}};
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  // start Wi-Fi
  ESP_ERROR_CHECK(esp_wifi_start());

  // prepare client
  esp_osc_client_init(&client, 1024, 9000);

  // create tasks
  xTaskCreatePinnedToCore(sender, "sender", 4096, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(receiver, "receiver", 4096, NULL, 10, NULL, 1);
}
