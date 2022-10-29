#ifndef ESP_OSC_H
#define ESP_OSC_H

#include <stdbool.h>
#include <stdint.h>
#include <lwip/netdb.h>
#include <tinyosc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char *sbuf;
  char *rbuf;
  size_t len;
  int socket;
} esp_osc_client_t;

typedef struct {
  struct sockaddr_in addr;
} esp_osc_target_t;

typedef struct {
  union {
    int32_t i;
    int64_t h;
    float f;
    double d;
    const char *s;
    const char *b;
  };
  int bl;
} esp_osc_value_t;

typedef bool (*esp_osc_callback_t)(const char *topic, const char *format, esp_osc_value_t *values);

bool esp_osc_init(esp_osc_client_t *client, uint16_t buf_len, uint16_t port);

esp_osc_target_t esp_osc_target(const char *address, uint16_t port);

bool esp_osc_send(esp_osc_client_t *client, esp_osc_target_t *target, const char *topic, const char *format, ...);

bool esp_osc_receive(esp_osc_client_t *client, esp_osc_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif  // ESP_OSC_H
