/**
 *
 *   Login service
 *
 */
#include "sdk_common.h"
#if NRF_MODULE_ENABLED(BLE_LGS)
#include "ble_lgs.h"
#include "ble_srv_common.h"

#include "nrf_log.h"
#include "nrf_delay.h"


/**@brief Function for handling the Write event.
 *
 * @param[in] p_lgs      login service structure.
 * @param[in] p_ble_evt  Event received from the BLE stack.
 */
static void on_write_request(ble_lgs_t * p_lgs, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.authorize_request.request.write;

/*
    NRF_LOG_INFO("Hexdump of received data:");
    NRF_LOG_HEXDUMP_INFO(p_evt_write->data, p_evt_write->len);
*/

    if (   (p_evt_write->handle == p_lgs->pass_input_char_handles.value_handle)
        && (p_lgs->password_input_handler != NULL))
    {
        p_lgs->password_input_handler(p_ble_evt->evt.gap_evt.conn_handle, p_lgs, p_evt_write);
    }

    if (   (p_evt_write->handle == p_lgs->pass_change_char_handles.value_handle)
        && (p_lgs->password_change_handler != NULL))
    {
        p_lgs->password_change_handler(p_ble_evt->evt.gap_evt.conn_handle, p_lgs, p_evt_write);
    }
}


void ble_lgs_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_lgs_t * p_lgs = (ble_lgs_t *)p_context;

    switch (p_ble_evt->header.evt_id)
    {

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
            if ( p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE  )
            {
                on_write_request(p_lgs, p_ble_evt);
            }
            break;



        default:
            // No implementation needed.
            break;
    }
}


uint32_t ble_lgs_init(ble_lgs_t * p_lgs, const ble_lgs_init_t * p_lgs_init)
{
    uint32_t                  err_code;
    ble_uuid_t                ble_uuid;
    ble_add_char_params_t     add_char_params;
    ble_add_char_user_desc_t  user_descriptor;

    // Initialize service structure.
    p_lgs->password_input_handler  = p_lgs_init->password_input_handler;
    p_lgs->password_change_handler = p_lgs_init->password_change_handler;

    // Add service.
    ble_uuid128_t base_uuid = {MASTER_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_lgs->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_lgs->uuid_type;
    ble_uuid.uuid = LGS_UUID_SERVICE;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_lgs->service_handle);
    VERIFY_SUCCESS(err_code);


    // Add password input characteristic.
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char input_desc[]           = "Password input";
    user_descriptor.p_char_user_desc  = (uint8_t *) input_desc;
    user_descriptor.max_size          = strlen(input_desc);
    user_descriptor.size              = strlen(input_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = LGS_UUID_PASS_INPUT;
    add_char_params.uuid_type        = p_lgs->uuid_type;
    add_char_params.init_len         = 20;
    add_char_params.max_len          = 20;
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_NO_ACCESS;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_defered_write = 1;

    err_code = characteristic_add(p_lgs->service_handle, &add_char_params, &p_lgs->pass_input_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }


    // Add password change characteristic.
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char change_desc[]         = "Password change";
    user_descriptor.p_char_user_desc  = (uint8_t *) change_desc;
    user_descriptor.max_size          = strlen(change_desc);
    user_descriptor.size              = strlen(change_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = LGS_UUID_PASS_CHANGE;
    add_char_params.uuid_type        = p_lgs->uuid_type;
    add_char_params.init_len         = 20;
    add_char_params.max_len          = 20;
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_NO_ACCESS;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_defered_write = 1;

    return characteristic_add(p_lgs->service_handle, &add_char_params, &p_lgs->pass_change_char_handles);


}



#endif // NRF_MODULE_ENABLED(BLE_lgs)
