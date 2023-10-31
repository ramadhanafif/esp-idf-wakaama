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

#include "m2m_platform.h"

static const char *TAG = "client";

#define HOST_IP_ADDR CONFIG_M2M_HOST
#define PORT         CONFIG_M2M_PORT

typedef struct con {
    int sock;
    struct sockaddr_in dest_addr;
    int dest_addr_size;
} con_t;

static con_t conn;

void *lwm2m_connect_server(uint16_t secObjInstID, void *userData)
{
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
    int err     = sendto(conn->sock, buffer, length, 0, (struct sockaddr *)&conn->dest_addr, conn->dest_addr_size);
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return COAP_412_PRECONDITION_FAILED;
    }
    output_buffer(buffer, length, 1);
    return COAP_NO_ERROR;
}

bool lwm2m_session_is_equal(void *session1, void *session2, void *userData)
{
    con_t *conn1 = (con_t *)session1;
    con_t *conn2 = (con_t *)session2;
    return conn1->sock == conn2->sock;
}

#define OBJ_COUNT 3
lwm2m_context_t *lwm2mH = NULL;
lwm2m_object_t *objArray[OBJ_COUNT];

extern lwm2m_object_t *get_security_object();
extern lwm2m_object_t *get_server_object();
extern lwm2m_object_t *get_object_device();

static uint8_t rx_buffer[1024];

static void client_task(void *pvParameters)
{
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
        vTaskDelete(NULL);
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

    objArray[0] = get_security_object();
    objArray[1] = get_server_object();
    objArray[2] = get_object_device();

    lwm2mH = lwm2m_init(NULL);

    int res = lwm2m_configure(lwm2mH, CONFIG_EP_NAME, NULL, NULL, OBJ_COUNT, objArray);
    if (res != 0) {
        ESP_LOGE(TAG, "lwm2m_configure failed");
    }

    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t socklen = sizeof(source_addr);
    while (1) {
        time_t tv = lwm2m_gettime();
        lwm2m_step(lwm2mH, &tv);

        int len = recvfrom(conn.sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);

        if (len > 0) { // this stupid function can return -1 on error and causes memory leak on output_buffer!!
            ESP_LOGI(TAG, "Received %d bytes", len);
            lwm2m_handle_packet(lwm2mH, rx_buffer, len, &conn);
            output_buffer(rx_buffer, len, 0);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
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

    xTaskCreate(client_task, "client_lwm2m", 4096, NULL, 5, NULL);
}
