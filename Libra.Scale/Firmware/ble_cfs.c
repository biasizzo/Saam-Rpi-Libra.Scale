/**
 *
 *   Configuration service
 *
 */
#include "sdk_common.h"
#if NRF_MODULE_ENABLED(BLE_CFS)
#include "ble_cfs.h"
#include "ble_srv_common.h"

#include "nrf_log.h"
#include "nrf_delay.h"


static char default_get_settings_value[]    = "Unauthorised";

/**@brief Function for handling the Write event.
 *
 * @param[in] p_cfs      Configuration Service structure.
 * @param[in] p_ble_evt  Event received from the BLE stack.
 */
static void on_write_request(ble_cfs_t * p_cfs, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.authorize_request.request.write;

    if (   (p_evt_write->handle == p_cfs->tare_char_handles.value_handle)
        && (p_cfs->tare_handler != NULL))
    {
        p_cfs->tare_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs, p_evt_write);
    }

    if (   (p_evt_write->handle == p_cfs->power_off_char_handles.value_handle)
        && (p_cfs->power_off_handler != NULL))
    {
        p_cfs->power_off_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs, p_evt_write);
    }

    if (   (p_evt_write->handle == p_cfs->set_refresh_time_char_handles.value_handle)
        && (p_cfs->set_refresh_time_handler != NULL))
    {
        p_cfs->set_refresh_time_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs, p_evt_write);
    }

    if (   (p_evt_write->handle == p_cfs->set_refresh_notify_char_handles.value_handle)
        && (p_cfs->set_refresh_notify_handler != NULL))
    {
        p_cfs->set_refresh_notify_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs, p_evt_write);
    }

    if (   (p_evt_write->handle == p_cfs->set_turn_off_timeout_char_handles.value_handle)
        && (p_cfs->set_turn_off_timeout_handler != NULL))
    {
        p_cfs->set_turn_off_timeout_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs, p_evt_write);
    }

    if (   (p_evt_write->handle == p_cfs->set_correction_char_handles.value_handle)
        && (p_cfs->set_correction_handler != NULL))
    {
        p_cfs->set_correction_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs, p_evt_write);
    }

    if (   (p_evt_write->handle == p_cfs->set_gain_rate_char_handles.value_handle)
        && (p_cfs->set_gain_rate_handler != NULL))
    {
        p_cfs->set_gain_rate_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs, p_evt_write);
    }


    if (   (p_evt_write->handle == p_cfs->change_name_char_handles.value_handle)
        && (p_cfs->change_name_handler != NULL))
    {
        p_cfs->change_name_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs, p_evt_write);
    }

    if (   (p_evt_write->handle == p_cfs->save_settings_char_handles.value_handle)
        && (p_cfs->save_settings_handler != NULL))
    {
        p_cfs->save_settings_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs, p_evt_write);
    }

}
static void on_read_request(ble_cfs_t * p_cfs, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_read_t const * p_evt_read = &p_ble_evt->evt.gatts_evt.params.authorize_request.request.read;
 
    if ( (p_evt_read->handle == p_cfs->get_settings_char_handles.value_handle)
      && (p_cfs->get_settings_handler != NULL))
    {
        p_cfs->get_settings_handler(p_ble_evt->evt.gap_evt.conn_handle, p_cfs);
    }




}


void ble_cfs_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    
    if ((p_context == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    ble_cfs_t * p_cfs = (ble_cfs_t *)p_context;


   switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:

            if ( p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE  )
            {
                on_write_request(p_cfs, p_ble_evt);
            }
            if ( p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_READ  )
            {
                on_read_request(p_cfs, p_ble_evt);
            }
        break;

        default:
            // No implementation needed.
            break;
    }
}


uint32_t ble_cfs_init(ble_cfs_t * p_cfs, const ble_cfs_init_t * p_cfs_init)
{
    uint32_t              err_code;
    ble_uuid_t            ble_uuid;
    ble_add_char_params_t     add_char_params;
    ble_add_char_user_desc_t  user_descriptor;

    // Initialize service structure.
    p_cfs->tare_handler = p_cfs_init->tare_handler;
    p_cfs->power_off_handler = p_cfs_init->power_off_handler;
    p_cfs->set_refresh_time_handler = p_cfs_init->set_refresh_time_handler;
    p_cfs->set_refresh_notify_handler = p_cfs_init->set_refresh_notify_handler;
    p_cfs->set_turn_off_timeout_handler = p_cfs_init->set_turn_off_timeout_handler;
    p_cfs->set_correction_handler = p_cfs_init->set_correction_handler;
    p_cfs->set_gain_rate_handler = p_cfs_init->set_gain_rate_handler;
    p_cfs->change_name_handler = p_cfs_init->change_name_handler;
    p_cfs->save_settings_handler = p_cfs_init->save_settings_handler;
    p_cfs->get_settings_handler = p_cfs_init->get_settings_handler;

    // Add service.
    ble_uuid128_t base_uuid = {MASTER_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_cfs->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_cfs->uuid_type;
    ble_uuid.uuid = CFS_UUID_SERVICE;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_cfs->service_handle);
    VERIFY_SUCCESS(err_code);


    // Add get settings characteristic.
    // The get settings characteristic is updated on login and every time a setting is written.
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char get_settings_desc[]   = "Get settings";
    user_descriptor.p_char_user_desc  = (uint8_t *) get_settings_desc;
    user_descriptor.max_size          = strlen(get_settings_desc);
    user_descriptor.size              = strlen(get_settings_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = CFS_UUID_GET_SETTINGS;
    add_char_params.uuid_type         = p_cfs->uuid_type;
    add_char_params.char_props.read   = 1;
    add_char_params.p_user_descr      = &user_descriptor;
    add_char_params.read_access       = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;
    add_char_params.is_value_user     = 0;
    add_char_params.is_var_len        = 1;
    add_char_params.max_len           = 40;
    add_char_params.init_len          = strlen(default_get_settings_value);
    add_char_params.p_init_value      = default_get_settings_value;
    add_char_params.is_defered_read   = 1;


    err_code = characteristic_add(p_cfs->service_handle,
                                  &add_char_params,
                                  &p_cfs->get_settings_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    

    // Add tare characteristic.
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char tare_desc[]           = "Tare";
    user_descriptor.p_char_user_desc  = (uint8_t *) tare_desc;
    user_descriptor.max_size          = strlen(tare_desc);
    user_descriptor.size              = strlen(tare_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = CFS_UUID_TARE;
    add_char_params.uuid_type        = p_cfs->uuid_type;
    add_char_params.init_len         = sizeof(uint8_t);
    add_char_params.max_len          = sizeof(uint8_t);
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_OPEN;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_value_user    = 0;
    add_char_params.is_var_len       = 0;
    add_char_params.is_defered_write = 1;

  
    err_code = characteristic_add(p_cfs->service_handle, &add_char_params, &p_cfs->tare_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }


    // Add power off characteristic.
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char power_off_desc[]           = "Power Off";
    user_descriptor.p_char_user_desc  = (uint8_t *) power_off_desc;
    user_descriptor.max_size          = strlen(power_off_desc);
    user_descriptor.size              = strlen(power_off_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = CFS_UUID_POWER_OFF;
    add_char_params.uuid_type        = p_cfs->uuid_type;
    add_char_params.init_len         = sizeof(uint8_t);
    add_char_params.max_len          = sizeof(uint8_t);
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_OPEN;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_value_user    = 0;
    add_char_params.is_var_len       = 0;
    add_char_params.is_defered_write = 1;

    err_code = characteristic_add(p_cfs->service_handle, &add_char_params, &p_cfs->power_off_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add set refresh time characteristic.
    // Expect ascii string between '62' and '5000'
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char refresh_time_desc[]   = "Set refresh time";
    user_descriptor.p_char_user_desc  = (uint8_t *) refresh_time_desc;
    user_descriptor.max_size          = strlen(refresh_time_desc);
    user_descriptor.size              = strlen(refresh_time_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = CFS_UUID_SET_REFRESH_TIME;
    add_char_params.uuid_type        = p_cfs->uuid_type;
    add_char_params.init_len         = 4;
    add_char_params.max_len          = 4;
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_OPEN;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_value_user    = 0;
    add_char_params.is_var_len       = 0;
    add_char_params.is_defered_write = 1;

    err_code = characteristic_add(p_cfs->service_handle, &add_char_params, &p_cfs->set_refresh_time_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add set refresh time notify characteristic.
    // Expect ascii string between '62' and '5000'
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char refresh_time_ind_desc[]   = "Set notification interval";
    user_descriptor.p_char_user_desc  = (uint8_t *) refresh_time_ind_desc;
    user_descriptor.max_size          = strlen(refresh_time_ind_desc);
    user_descriptor.size              = strlen(refresh_time_ind_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = CFS_UUID_SET_REFRESH_NOTIFY;
    add_char_params.uuid_type        = p_cfs->uuid_type;
    add_char_params.init_len         = 4;
    add_char_params.max_len          = 4;
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_OPEN;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_value_user    = 0;
    add_char_params.is_var_len       = 0;
    add_char_params.is_defered_write = 1;

    err_code = characteristic_add(p_cfs->service_handle, &add_char_params, &p_cfs->set_refresh_notify_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }


    // Add turn off timeout characteristic.
    // Expect ascii string between '0' and '60'
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char turn_off_timeout_desc[]           = "Set turn off timeout";
    user_descriptor.p_char_user_desc  = (uint8_t *) turn_off_timeout_desc;
    user_descriptor.max_size          = strlen(turn_off_timeout_desc);
    user_descriptor.size              = strlen(turn_off_timeout_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = CFS_UUID_SET_TURN_OFF_TIMEOUT;
    add_char_params.uuid_type        = p_cfs->uuid_type;
    add_char_params.init_len         = 2;
    add_char_params.max_len          = 2;
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_OPEN;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_value_user    = 0;
    add_char_params.is_var_len       = 0;
    add_char_params.is_defered_write = 1;

    err_code = characteristic_add(p_cfs->service_handle, &add_char_params, &p_cfs->set_turn_off_timeout_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add save settings characteristic.
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char save_settings_desc[]           = "Save settings";
    user_descriptor.p_char_user_desc  = (uint8_t *) save_settings_desc;
    user_descriptor.max_size          = strlen(save_settings_desc);
    user_descriptor.size              = strlen(save_settings_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = CFS_UUID_SAVE_SETTINGS;
    add_char_params.uuid_type        = p_cfs->uuid_type;
    add_char_params.init_len         = sizeof(uint8_t);
    add_char_params.max_len          = sizeof(uint8_t);
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_OPEN;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_value_user    = 0;
    add_char_params.is_defered_write = 1;

    err_code = characteristic_add(p_cfs->service_handle, &add_char_params, &p_cfs->save_settings_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add correction factor characteristic.
    // Expect 4bytes = int32
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char correction_factor_desc[]           = "Set correction factor";
    user_descriptor.p_char_user_desc  = (uint8_t *) correction_factor_desc;
    user_descriptor.max_size          = strlen(correction_factor_desc);
    user_descriptor.size              = strlen(correction_factor_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = CFS_UUID_SET_CORRECTION;
    add_char_params.uuid_type        = p_cfs->uuid_type;
    add_char_params.init_len         = 4;
    add_char_params.max_len          = 4;
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_OPEN;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_value_user    = 0;
    add_char_params.is_defered_write = 1;

    err_code = characteristic_add(p_cfs->service_handle, &add_char_params, &p_cfs->set_correction_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add gain rate characteristic.
    // Expect 1byte
    // bit0 and bit1 -> select gain 0 = 32;   1=64;   2=128
    // bit2          -> select rate 0 = 10hz; 1=80hz

    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char gain_rate_desc[]      = "Set HX711 gain and rate. bit 0-1: gain 32;64;128, bit 2: rate 10;80";
    user_descriptor.p_char_user_desc  = (uint8_t *) gain_rate_desc;
    user_descriptor.max_size          = strlen(gain_rate_desc);
    user_descriptor.size              = strlen(gain_rate_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = CFS_UUID_SET_GAIN_RATE;
    add_char_params.uuid_type        = p_cfs->uuid_type;
    add_char_params.init_len         = 1;
    add_char_params.max_len          = 1;
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_OPEN;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_value_user    = 0;
    add_char_params.is_defered_write = 1;

    err_code = characteristic_add(p_cfs->service_handle, &add_char_params, &p_cfs->set_gain_rate_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }


    // Add change name characteristic.
    // max length of name = 20
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char change_name_desc[]    = "Change name";
    user_descriptor.p_char_user_desc  = (uint8_t *) change_name_desc;
    user_descriptor.max_size          = strlen(change_name_desc);
    user_descriptor.size              = strlen(change_name_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = CFS_UUID_CHANGE_NAME;
    add_char_params.uuid_type        = p_cfs->uuid_type;
    add_char_params.init_len         = 20;
    add_char_params.max_len          = 20;
    add_char_params.char_props.read  = 0;
    add_char_params.char_props.write = 1;
    add_char_params.p_user_descr     = &user_descriptor;
    add_char_params.read_access      = SEC_OPEN;
    add_char_params.write_access     = SEC_OPEN;
    add_char_params.is_value_user    = 0;
    add_char_params.is_defered_write = 1;

    return characteristic_add(p_cfs->service_handle, &add_char_params, &p_cfs->change_name_char_handles);

}


#endif // NRF_MODULE_ENABLED(BLE_cfs)
