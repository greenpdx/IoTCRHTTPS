/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_check.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "time_sync.h"

static const char *TAG = "time_sync";

#define STORAGE_NAMESPACE "storage"


void time_sync_notification_cb(struct timeval *tv)
{
    time_t tm = tv->tv_sec;
    nvs_handle_t my_handle = 0;
    esp_err_t err;
    char strftime_buf[64];
    struct tm timeinfo;

    ESP_LOGI(TAG, "Notification of a time synchronization event");

    setenv("TZ", "CST6", 1);
    tzset();
    localtime_r(&tm, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "SYNC date/time in Costa Rica is: %s", strftime_buf);

    //Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        goto exit;
    }

    //Write
    err = nvs_set_i64(my_handle, "timestamp", tm);
    if (err != ESP_OK) {
        goto exit;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        goto exit;
    }

exit:
    if (my_handle != 0) {
        nvs_close(my_handle);
    }
   // esp_netif_sntp_deinit();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error updating time in nvs");
    } else {
        ESP_LOGI(TAG, "Updated time in NVS");
    }
}

esp_err_t initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(0, {} );
//                               ESP_SNTP_SERVER_LIST("10.42.0.1" ) );
    config.server_from_dhcp = true;
    config.start = false;
    config.renew_servers_after_new_IP = true;
    config.index_of_first_server = 1;
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
    config.sync_cb = time_sync_notification_cb;  
    return esp_netif_sntp_init(&config);
}


esp_err_t start_sntp(void) {
    ESP_RETURN_ON_ERROR(esp_netif_sntp_start(), TAG, "SNTP start error");
    ESP_RETURN_ON_ERROR(fetch_and_store_time_in_nvs(NULL), TAG, "Get and set time Failed");
    return ESP_OK;
}

static esp_err_t obtain_time(void)
{
    // wait for time to be set
    int retry = 0;
    const int retry_count = 10;
    ESP_LOGI(TAG, "SNTP Obtain time");

    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) != ESP_OK && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    if (retry == retry_count) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t fetch_and_store_time_in_nvs(void *args)
{
    nvs_handle_t my_handle = 0;
    esp_err_t err;

    //initialize_sntp();
    if (obtain_time() != ESP_OK) {
        err = ESP_FAIL;
        goto exit;
    }

    time_t now;
    time(&now);

    //Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        goto exit;
    }

    //Write
    err = nvs_set_i64(my_handle, "timestamp", now);
    if (err != ESP_OK) {
        goto exit;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        goto exit;
    }

exit:
    if (my_handle != 0) {
        nvs_close(my_handle);
    }
    esp_netif_sntp_deinit();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error updating time in nvs");
    } else {
        ESP_LOGI(TAG, "Updated time in NVS");
    }
    return err;
}

esp_err_t update_time_from_nvs(void)
{
    nvs_handle_t my_handle = 0;
    esp_err_t err;
    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;

    time(&now);
    // Set timezone to Costa Rica Standard Time
    setenv("TZ", "UTC", 1);
    tzset();

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Costa Rica is: %s", strftime_buf);

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS");
        goto exit;
    }

    int64_t timestamp = 0;

    //err = nvs_get_i64(my_handle, "timestamp", &timestamp);
    //if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Time not found in NVS. Syncing time from SNTP server.");
        if (fetch_and_store_time_in_nvs(NULL) != ESP_OK) {
            err = ESP_FAIL;
        } else {
            err = ESP_OK;
        }
    //} else if (err == ESP_OK) {
    //    struct timeval get_nvs_time;
    //    get_nvs_time.tv_sec = timestamp;
    //    settimeofday(&get_nvs_time, NULL);
    //}

exit:
    if (my_handle != 0) {
        nvs_close(my_handle);
    }
    time(&now);
    // Set timezone to Costa Rica Standard Time
    setenv("TZ", "UTC-6", 1);
    tzset();

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The after date/time in Costa Rica is: %s", strftime_buf);

    return err;
}
