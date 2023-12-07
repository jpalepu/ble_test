#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"
#include "esp_bt.h"

const char *TAG = "BLE-Server";
uint8_t ble_addr;
uint16_t notification_handle=0;
uint16_t conn_handle;
TimerHandle_t cnt_timer;
int count, start_time, current_time;
uint16_t waitTime; //variable to wait for 20 minutes to have some connection else Nimble stack shutdown


bool conn_state         = false;
bool conn_prev_state    = false;

static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int device_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ble_gap_event(struct ble_gap_event *event, void *arg); //this is just to notify an event, it can be connection or anything. 
static int ble_app_advertise(void);
static void cnt_reset(void);


// BLE GATT service definition: write from the client to the server. 
static const struct ble_gatt_chr_def gatt_characteristics[] = {
    {
        .uuid = BLE_UUID16_DECLARE(0xABCD),
        .flags = BLE_GATT_CHR_F_WRITE,
        .access_cb = device_write,
    },

    {
        .uuid = BLE_UUID16_DECLARE(0xEACD),
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &notification_handle,
        .access_cb = device_read,
    },

    {0}, // End of characteristics
};

// BLE GATT service definition: has one service that consists of one characteristic i.e write from client. 
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY, //primary service
        .uuid = BLE_UUID16_DECLARE(0x007), 
        .characteristics = gatt_characteristics,
    },
    {0}, // End of services
}; 

static void notify_device(TimerHandle_t ev){
    struct os_mbuf *om;
    int rc;
    count++;

    if(notification_handle==0){
        cnt_reset();
        return;
    }

    om = ble_hs_mbuf_from_flat(&count, sizeof(count));
    rc = ble_gatts_notify_custom((uint16_t)conn_handle, notification_handle, om);
    assert(rc == 0);
    if (rc != 0) {
        ESP_LOGE(TAG, "error notifying; rc = %d", rc);
    }
    cnt_reset();
}

static void cnt_reset(void)
{
    int rc;
    if (xTimerReset(cnt_timer, 1000 / portTICK_PERIOD_MS ) == pdPASS) {
        rc = 0;
    } 
    else {
        rc = 1;
    }
    assert(rc == 0);
}

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);
    return 0;
}

// Read data from ESP32 defined as server
static int device_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    os_mbuf_append(ctxt->om, "test send/read from server", strlen("send/read test server"));
    return 0;
}

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg) 
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status == 0) {
            conn_state = true;
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE GAB DISCONNECT REASON: %d", event->disconnect.reason);
        ble_app_advertise();
        conn_state = false;
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE: //complete discovery procedure
        ESP_LOGI(TAG, "BLE GAP EVENT FINISHED...");
        ble_app_advertise();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI("BLE_GAP_SUBSCRIBE_EVENT", "conn_handle from subscribe=%d", conn_handle);
        break;
    default:
        break;
    }
    return 0;
}


// Define the BLE connection, then use the field to adverstise the data. this is more like structuring the packet to adverstise
 int ble_app_advertise(void)
{
    struct ble_hs_adv_fields fields;
    const char *device_name;

    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; //undirected connection mode to accept req from any peer device. 
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;  //this means it is general discoverable (a peripheral device can be seen by a central device as long as it is advertising)

    ESP_LOGI(TAG, " ADVERTISING...");
    uint8_t ret = ble_gap_adv_start(ble_addr, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    return ret;

}

int ble_stop_advertise(){

    ESP_LOGI(TAG, "Stopping advertising");
    ble_gap_adv_stop();
    return 0;
}

// BLE application synchronization callback
static void ble_app_on_sync(void)
{
   //struct ble_gap_event *event; && event->connect.status != 0
    ble_hs_id_infer_auto(0, &ble_addr);
    start_time = esp_timer_get_time();
    ble_app_advertise();
}
// Host task
static void host_task(void *param)
{
    nimble_port_run();
}

void ble_init(){

    nvs_flash_init();
    esp_nimble_hci_init();
    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("BLE-Server");
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    nimble_port_freertos_init(host_task);

}

void ble_deinit(){

    ESP_LOGI(TAG, "DEINITIALIZING BLE...");
    nimble_port_deinit();
    esp_nimble_hci_deinit();
    nvs_flash_deinit();

}


void app_main(){

    uint64_t time = 65 * 1000 * 1000;
    uint8_t ret;
    ble_init();

    ESP_LOGI(TAG, "Initialized the Ble-Server");
    
    // cnt_timer = xTimerCreate("count timer", pdMS_TO_TICKS(20000), pdTRUE, 0, notify_device);
    // cnt_reset();

    current_time = esp_timer_get_time();
   
    while((start_time - current_time) < 100000){
        
        vTaskDelay(pdMS_TO_TICKS(100));
    
        
        if(conn_state && conn_state != conn_prev_state) {
            conn_prev_state = conn_state;
            ESP_LOGI(TAG, "Connection Made!");
        }
        
        if(!conn_state && conn_state != conn_prev_state){
            conn_prev_state = conn_state;
            ESP_LOGI(TAG, "Disconnected!");
        }
    }
    ble_stop_advertise();
    ESP_LOGI(TAG, "Stopped Advertising..");
}














