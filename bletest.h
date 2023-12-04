#pragma once

#include <stdint.h>
#include "esp_err.h"

namespace irreo::iot_services

{ 

    /// @brief Initialize Ble Nimble Service
    /// @return ESP_OK on success.
    /// @note this function is thread safe.
    esp_err_t ble_init();

    /// @brief Deinitializes all the ble service layers and modules.
    /// @return ESP_OK on success.
    /// @note this function is NOT thread safe and must me called only one time during deinitialization!
    esp_err_t ble_deinit();

    /// @brief used to define the ble connection and uses "fields to advertise".
    /// @return 0 on successfull connection: starts advertising. 
    /// @note A valid result is returned only after ble_init() has been called, otherwise advertising will not work. 
    static int ble_app_advertise(void);

    /// @brief Used for BLE event handling (connect/disconncted/subscribed)
    /// @param event Represents a GAP-related event.
    /// @return 0 on success.
    /// @note this function is thread safe.
    static int ble_gap_event(struct ble_gap_event *event, void *arg); 

    /// @brief This function is to write the data from the peripheral into the server.  
    /// @param conn_handle connection handle for the peripheral
    /// @param attr_handle
    /// @param ctxt data that has to be written (from the os buffer)
    /// @param arg 
    /// @return ESP_OK on success 
    /// @note Server is only capable of writing the data. We cannot read the data of the server.
    esp_err_t device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

    /// @brief This function will read the data to the client from the peripherals
    /// @param conn_handle 
    /// @param attr_handle 
    /// @param ctxt 
    /// @param arg 
    /// @return ESP_OK on success 
    esp_err_t device_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

    /// @brief This function is synchonization (host and controller) callback does automatic address calculation (if not mentioned. This happens after startup/reset. 
    /// @return ESP_OK on success
    esp_err_t ble_app_on_sync(void);

    /// @brief this function notifies the data to the client
    /// @param ev timer handle
    /// @note at the time of testing, parallel execution of reading data from peripheral and notification were not tested. 
    static void notify_device(TimerHandle_t ev);


}
