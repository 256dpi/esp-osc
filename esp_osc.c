#include <stdio.h>
#include <stdarg.h>
#include <esp_log.h>

#include "esp_osc.h"

#define TAG "esp-osc"

bool esp_osc_init(esp_osc_client_t *client, uint16_t buf_len, uint16_t port) {
  // free existing memory
  if (client->sbuf != NULL) {
    free(client->sbuf);
  }
  if (client->rbuf != NULL) {
    free(client->rbuf);
  }

  // allocate memory
  client->sbuf = malloc(buf_len);
  client->rbuf = malloc(buf_len);
  client->len = buf_len;

  // close existing socket
  if (client->socket != 0) {
    close(client->socket);
    client->socket = 0;
  }

  // create socket
  client->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (client->socket <= 0) {
    ESP_LOGE(TAG, "failed to create socket (%d)", errno);
    client->socket = 0;
    return false;
  }

  // bind socket if port is available
  if (port > 0) {
    struct sockaddr_in addr = {0};
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (bind(client->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      ESP_LOGE(TAG, "failed to bind socket (%d)", errno);
      return false;
    }
  }

  return true;
}

esp_osc_target_t esp_osc_target(const char *address, uint16_t port) {
  // prepare target
  esp_osc_target_t target = {0};
  target.addr.sin_addr.s_addr = inet_addr(address);
  target.addr.sin_family = AF_INET;
  target.addr.sin_port = htons(port);

  return target;
}

bool esp_osc_send(esp_osc_client_t *client, esp_osc_target_t *target, const char *topic, const char *format, ...) {
  // send message
  va_list args;
  va_start(args, format);
  bool ret = esp_osc_send_v(client, target, topic, format, args);
  va_end(args);

  return ret;
}

bool esp_osc_send_v(esp_osc_client_t *client, esp_osc_target_t *target, const char *topic, const char *format,
                    va_list args) {
  // prepare message
  uint32_t length = tosc_vwrite((char *)client->sbuf, client->len, topic, format, args);

  // send message
  if (sendto(client->socket, client->sbuf, length, 0, (struct sockaddr *)&target->addr, sizeof(target->addr)) < 0) {
    ESP_LOGE(TAG, "failed to send message (%d)", errno);
    return false;
  }

  return true;
}

bool esp_osc_receive(esp_osc_client_t *client, esp_osc_callback_t callback) {
  // prepare values
  esp_osc_value_t values[32];

  for (;;) {
    // receive message
    ssize_t ret = recvfrom(client->socket, client->rbuf, client->len, 0, NULL, NULL);
    if (ret < 0) {
      ESP_LOGE(TAG, "failed to receive message (%d)", errno);
      return false;
    } else if (ret > client->len) {
      ESP_LOGE(TAG, "discard too long message (%d)", ret);
      return false;
    }

    // check bundle
    if (tosc_isBundle(client->rbuf)) {
      ESP_LOGE(TAG, "discard unsupported bundle");
      return false;
    }

    // parse message
    tosc_message msg = {0};
    int res = tosc_parseMessage(&msg, client->rbuf, client->len);
    if (res < 0) {
      ESP_LOGE(TAG, "failed to parse message (%d)", res);
      return false;
    }

    // get format
    char *fmt = tosc_getFormat(&msg);

    // get size
    size_t size = strlen(fmt);
    if (size > sizeof(values)) {
      ESP_LOGE(TAG, "message has too many values (%d)", size);
      return false;
    }

    // parse values
    for (size_t i = 0; i < size; i++) {
      switch (fmt[i]) {
        case 'i':
          values[i].i = tosc_getNextInt32(&msg);
          break;
        case 'h':
          values[i].h = tosc_getNextInt64(&msg);
          break;
        case 'f':
          values[i].f = tosc_getNextFloat(&msg);
          break;
        case 'd':
          values[i].d = tosc_getNextDouble(&msg);
          break;
        case 's':
          values[i].s = tosc_getNextString(&msg);
          break;
        case 'b':
          tosc_getNextBlob(&msg, &values[i].b, &values[i].bl);
          break;
      }
    }

    // call callback
    if (!callback(tosc_getAddress(&msg), fmt, values)) {
      return false;
    }
  }

  return true;
}
