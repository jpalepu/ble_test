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
//#include "bletest.h"


static const char *TAG = "BLE-Module";

static int count;
uint16_t waitTime; //variable to wait for 20 minutes to have some connection else Nimble stack shutdown
static uint8_t ble_addr;
static uint16_t conn_handle;
static TimerHandle_t cnt_timer;
static uint16_t notification_handle=0;

static void cnt_reset(void);
esp_err_t device_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
esp_err_t device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ble_gap_event(struct ble_gap_event *event, void *arg); //this is just to notify an event, it can be connection or anything. 
static int ble_app_advertise(void);


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
esp_err_t device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);
    return ESP_OK;
}

// Read data from ESP32 defined as server
esp_err_t device_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    os_mbuf_append(ctxt->om, "test send/read from server", strlen("send/read test server"));
    return ESP_OK;
}

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg) 
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
            ble_app_advertise();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE: //complete discovery procedure
        ESP_LOGI(TAG, "BLE GAP EVENT");
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
static int ble_app_advertise(void)
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

    uint8_t ret = ble_gap_adv_start(ble_addr, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    
    return ret;

}

esp_err_t ble_deinit(){
   
    ESP_LOGI(TAG, "Ble Stack is shutting down as there is no conn!");
    nvs_flash_deinit();
    esp_nimble_hci_deinit();
    nimble_port_deinit();
    //ble_hs_shutdown(BLE_GAP_EVENT_CONNECT);
    //esp_bt_sleep_enable();
    return ESP_OK;

}

// BLE application synchronization callback
esp_err_t ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr);
    ble_app_advertise();
 
    return ESP_OK;  
}

// Host task
static void host_task(void *param)
{
    nimble_port_run();
}

esp_err_t ble_init(){

    nvs_flash_init();
    esp_nimble_hci_init();
    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("BLE-Server");

    return ESP_OK;
}

//initialization of the hardware inside. 
void app_main()
{   
    ble_init();
    cnt_timer = xTimerCreate("count timer", pdMS_TO_TICKS(25000), pdTRUE, 0, notify_device);
    cnt_reset();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    nimble_port_freertos_init(host_task);
}













