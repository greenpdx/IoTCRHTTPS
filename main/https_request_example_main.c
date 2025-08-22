/*
 * HTTPS GET Example using plain Mbed TLS sockets
 *
 * Contacts the howsmyssl.com API via TLS v1.2 and reads a JSON
 * response.
 *
 * Adapted from the ssl_client1 example in Mbed TLS.
 *
 * SPDX-FileCopyrightText: The Mbed TLS Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2025 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "protocol_examples_common.h"
#include "esp_sntp.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_tls.h"
#include "sdkconfig.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE && CONFIG_EXAMPLE_USING_ESP_TLS_MBEDTLS
#include "esp_crt_bundle.h"
#endif
#include "time_sync.h"
#include "cJSON.h"

#include "http_parser.h"

/* Constants that aren't configurable in menuconfig */

#define WEB_SERVER "local"
#define WEB_PORT "443"
#define WEB_URL "https://10.42.0.1"
#define POST_URL "https://10.42.0.1/echo"

#define SERVER_URL_MAX_SZ 256

static const char *TAG = "example";

/* Timer interval once every day (24 Hours) */
#define TIME_PERIOD (86400000000ULL)

//method, URL, Host, content size, data
static const char REQUEST[] = "%s %s HTTP/1.1\r\n"
                             "Host: %s\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "Connection: close\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: %u\r\n"
                             "\r\n"
                             "%s";

/* Root cert for howsmyssl.com, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

static int build_request(const char *format, unsigned char * buf, const char *method, char *url, const char *data) {
    int ret = sprintf((char *)buf, format, method, url, WEB_SERVER, strlen(data), data);
    ESP_LOGI(TAG, "%u, %s", ret, buf);
    return ret;

}

static void https_request(esp_tls_cfg_t cfg, char *url, const char *REQUEST, char* data)
{
    unsigned char buf[8192];
    char *method = "GET";
    int ret, len, more;
    http_response_t res;

    esp_tls_t *tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        goto exit;
    }

    cfg.common_name = "local";
    if (strlen(data) > 0) {
        method = "POST";
    }

    ret = build_request(REQUEST, buf, method, url, data);

    if (esp_tls_conn_http_new_sync(url, &cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        int esp_tls_code = 0, esp_tls_flags = 0;
        esp_tls_error_handle_t tls_e = NULL;
        esp_tls_get_error_handle(tls, &tls_e);
        /* Try to get TLS stack level error and certificate failure flags, if any */
        ret = esp_tls_get_and_clear_last_error(tls_e, &esp_tls_code, &esp_tls_flags);
        if (ret == ESP_OK) {
            ESP_LOGE(TAG, "TLS error = -0x%x, TLS flags = -0x%x", esp_tls_code, esp_tls_flags);
        }
        goto cleanup;
    }

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
                                 buf + written_bytes,
                                 strlen((char*)buf) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto cleanup;
        }
    } while (written_bytes < strlen(REQUEST));

    ESP_LOGI(TAG, "Reading HTTP response...");
    len = sizeof(buf) - 1;
    memset(buf, 0x00, sizeof(buf));
    do {
        //len = sizeof(buf) - 1;
        //memset(buf, 0x00, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);
        ESP_LOGI(TAG, "TOTAL %s ", buf);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            continue;
        } else if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        } else if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        len = len - ret;
        ESP_LOGI(TAG, "%d bytes read", ret);

        more = esp_tls_get_bytes_avail(tls);
        if ( more > 0) continue;

        httpParseResponse(buf, &res);
        ESP_LOGI(TAG, "\t %i", res.num_headers);
        for (int i =0; i < res.num_headers; i++) {
            headers_kv_t * header = &res.headers[i];
                ESP_LOGI(TAG, "\t %s: %s", header->key, header->value);

        }

        unsigned char * ptr = httpGetResponseBody(&res);

        if (ptr == NULL) {
            ESP_LOGI(TAG,"Bad Response");
            break;
        }
        cJSON * json = cJSON_Parse((char *)ptr);
        if (json == NULL) {
            /* Print response directly to stdout as it is read */
            for (int i = 0; i < len; i++) {
                putchar(ptr[i]);
            }
            putchar('\n'); // JSON output doesn't have a newline at end
        } else {
                ESP_LOGI(TAG,"\n%s", cJSON_Print(json));
            cJSON_Delete(json);
        }

        //ESP_LOGD(TAG, "STRING %s", ptr);
        
    } while (1);

cleanup:
    esp_tls_conn_destroy(tls);
exit:
   
}

static void https_request_using_cacert_buf(void)
{
    ESP_LOGI(TAG, "https_request using cacert_buf");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) server_root_cert_pem_start,
        .cacert_bytes = (unsigned int) server_root_cert_pem_end - (unsigned int)server_root_cert_pem_start,
    };
    https_request(cfg, WEB_URL, REQUEST, "");

    cJSON *monitor = cJSON_CreateObject();

    if (cJSON_AddStringToObject(monitor, "name", "Awesome 4K") == NULL) {
        ;
    }
    char * string = cJSON_PrintUnformatted(monitor);
    https_request(cfg, POST_URL, REQUEST, string);
    cJSON_Delete(monitor);
    free(string);

     for (int countdown = 10; countdown >= 0; countdown--) {
        ESP_LOGI(TAG, "%d...", countdown);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void https_request_task(void *pvparameters)
{
    ESP_LOGI(TAG, "Start https_request example");

    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
    https_request_using_cacert_buf();
    ESP_LOGI(TAG, "Finish https_request example");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    
    //if (esp_reset_reason() == ESP_RST_POWERON) {
        ESP_LOGI(TAG, "Initalize DHCP sntp");
        ESP_ERROR_CHECK(initialize_sntp());
    //}

    const esp_timer_create_args_t nvs_update_timer_args = {
            .callback = (void *)&fetch_and_store_time_in_nvs,
    };

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    
    ESP_ERROR_CHECK(start_sntp());
    
    esp_timer_handle_t nvs_update_timer;
    ESP_ERROR_CHECK(esp_timer_create(&nvs_update_timer_args, &nvs_update_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(nvs_update_timer, TIME_PERIOD));

    xTaskCreate(&https_request_task, "https_get_task", 16384, NULL, 5, NULL);
}
