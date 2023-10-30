/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portable.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include <esp_timer.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <liblwm2m.h>

static const char *TAG     = "client";
static const char *payload = "Message from ESP32 ";

#define HOST_IP_ADDR CONFIG_M2M_HOST

#define PORT CONFIG_M2M_PORT

typedef struct con {
    int sock;
    struct sockaddr_in dest_addr;
    int dest_addr_size;
    struct timeval timeout;
} con_t;

static cont_t conn;

void *lwm2m_malloc(size_t s)
{
    return pvPortMalloc(s);
}

void lwm2m_free(void *p)
{
    vPortFree(p);
}

char *lwm2m_strdup(const char *str)
{
    return strdup(str);
}

// Compare at most the n first bytes of s1 and s2, return 0 if they match
int lwm2m_strncmp(const char *s1, const char *s2, size_t n)
{
    return strncmp(s1, s2, n);
}
int lwm2m_strcasecmp(const char *str1, const char *str2)
{
    return strcasecmp(str1, str2);
}

time_t lwm2m_gettime(void)
{
    return (time_t)esp_timer_get_time() / 1000000ULL;
}
// Get a seed (which must not repeat when the device reboots) for generating a random number
int lwm2m_seed(void)
{
    return (int)esp_random();
}

void *lwm2m_connect_server(uint16_t secObjInstID, void *userData)
{
    char host_ip[]  = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
    dest_addr.sin_family      = AF_INET;
    dest_addr.sin_port        = htons(PORT);
    addr_family               = AF_INET;
    ip_protocol               = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return NULL;
    }

    // Set timeout
    struct timeval timeout;
    timeout.tv_sec  = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    ESP_LOGI(TAG, "Socket created");

    conn.sock = sock;
    memcpy(&conn.dest_addr, &dest_addr, sizeof(dest_addr));
    conn.dest_addr_size = sizeof(dest_addr);
    memcpy(&conn.timeout, &timeout, sizeof(struct timeval));

    return &conn;
}

void lwm2m_close_connection(void *sessionH, void *userData)
{
    con_t *conn = (con_t *)sessionH;
    ESP_LOGE(TAG, "Shutting down socket and restarting...");
    shutdown(conn->sock, 0);
    close(conn->sock);
}

uint8_t lwm2m_buffer_send(void *sessionH, uint8_t *buffer, size_t length, void *userData)
{
    con_t *conn = (con_t *)sessionH;
    int err     = sendto(conn->sock, payload, length, 0, (struct sockaddr *)&conn->dest_addr, conn->dest_addr_size);
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return COAP_412_PRECONDITION_FAILED;
    }
    return COAP_NO_ERROR;
}
bool lwm2m_session_is_equal(void *session1, void *session2, void *userData)
{
    return session1 == session2;
}

static void udp_client_task(void *pvParameters)
{
    char rx_buffer[1024];

    while (1) {
        // int err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        // if (err < 0) {
        //     ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        //     break;
        // }
        ESP_LOGI(TAG, "Message sent");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
}
