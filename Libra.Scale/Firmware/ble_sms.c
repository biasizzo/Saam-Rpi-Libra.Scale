/**
 *
 *   Scale measurement service
 *
 */
#include "sdk_common.h"
#if NRF_MODULE_ENABLED(BLE_SMS)
#include "ble_sms.h"
#include "ble_srv_common.h"

#include "nrf_log.h"
#include "nrf_delay.h"

void ble_sms_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_sms_t * p_sms = (ble_sms_t *)p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTS_EVT_HVC: // INDICATE CONFIRMATION RECEIVED
            NRF_LOG_INFO("Indication confirmed");
            
        break;


case BLE_GATTC_EVT_HVX :
            NRF_LOG_INFO("Notification");
        break;

        case BLE_GATTS_EVT_TIMEOUT:
            NRF_LOG_INFO("Indication confirm timeout");
        break;




NRF_LOG_INFO("Notification confirmed");

        default:
            // No implementation needed.
            break;
    }
}


uint32_t ble_sms_init(ble_sms_t * p_sms)
{
    uint32_t                  err_code;
    ble_uuid_t                ble_uuid;
    ble_add_char_params_t     add_char_params;
    ble_add_char_user_desc_t  user_descriptor;

    // Add service.
    ble_uuid128_t base_uuid = {MASTER_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_sms->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_sms->uuid_type;
    ble_uuid.uuid = SMS_UUID_SERVICE;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_sms->service_handle);
    VERIFY_SUCCESS(err_code);



    //Create characteristic user description
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char user_desc[]           = "Measurement of weight in grams only - sent as an int, not string";
    user_descriptor.p_char_user_desc  = (uint8_t *) user_desc;
    user_descriptor.max_size          = strlen(user_desc);
    user_descriptor.size              = strlen(user_desc);
    user_descriptor.read_access       = 1;



    // Add (NOTIFY) characteristic.
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = SMS_UUID_WEIGHT_MEASUREMENT;
    add_char_params.uuid_type         = p_sms->uuid_type;
    add_char_params.char_props.read   = 0;
    add_char_params.char_props.indicate = 1;
    add_char_params.char_props.notify = 0;
    add_char_params.p_user_descr      = &user_descriptor;
    add_char_params.read_access       = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;
    add_char_params.is_var_len      = 0;
    add_char_params.max_len         = 6;
    add_char_params.init_len        = 6;

    return characteristic_add(p_sms->service_handle,
                                  &add_char_params,
                                  &p_sms->scale_measurement_char_handles);

}


uint32_t ble_sms_send_measurement(uint16_t conn_handle, ble_sms_t * p_sms, char * value)
{

    ble_gatts_hvx_params_t params;
    uint16_t len = 6;

    memset(&params, 0, sizeof(params));
    //params.type   = BLE_GATT_HVX_NOTIFICATION;
    params.type   = BLE_GATT_HVX_INDICATION;
    params.handle = p_sms->scale_measurement_char_handles.value_handle;
    params.p_data = value;
    params.p_len  = &len;     //arbitrary as characteristic is not variable length
    
    
    //NRF_LOG_INFO("Sending notification");
    return sd_ble_gatts_hvx(conn_handle, &params); //notify or indicate an attribute value
    
}


#endif // NRF_MODULE_ENABLED(BLE_sms)
