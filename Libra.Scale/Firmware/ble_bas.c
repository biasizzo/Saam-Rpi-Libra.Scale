/**
 *
 *   Battery service
 *
 */

#include "sdk_common.h"
#if NRF_MODULE_ENABLED(BLE_BAS)
#include "ble_bas.h"
#include <string.h>
#include <stdio.h>
#include "ble_srv_common.h"
#include "ble_conn_state.h"
#include "app_timer.h"

#include "nrf_log.h"
#include "nrf_drv_saadc.h"



#define AMUX_ADC                        NRF_SAADC_INPUT_AIN0


#define ADC_REF_VOLTAGE_IN_MILLIVOLTS   600               /**< Reference voltage (in milli volts) used by ADC while doing conversion. */
#define ADC_PRE_SCALING_COMPENSATION    6                 /**< The ADC is configured to use VDD with 1/3 prescaling as input. And hence the result of conversion is to be multiplied by 3 to get the actual value of the battery voltage.*/
#define ADC_RES_10BIT                   1024              /**< Maximum digital value for 10-bit ADC conversion. */


static nrf_saadc_value_t adc_buf[2];                     /** Buffer used by saadc implementation*/
static uint16_t    saadc_return_conn_handle;             /** Connection handle for saadc*/
static uint16_t    saadc_return_value_handle;            /** Value handle so saadc handler knows what value to return*/
static ble_bas_t * saadc_p_bas;                          /** initialized p_bas structure passed from main: for comparing value handles  */

/**@brief Macro to convert the result of ADC conversion in millivolts.
 */

#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)\
        ((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_PRE_SCALING_COMPENSATION)

/**@brief Converts MAX77734 MUX voltage to battery voltage
 */
#define MUX_RESULT_IN_BATT_VOLTAGE(MUX_VALUE)\
        ((MUX_VALUE * 4.6) / 1.25)

//Specify timer
APP_TIMER_DEF(m_timer_adc_read);

/* -------------------------------
 *              ADC
 * -------------------------------
 */

/**@brief Function for handling the ADC interrupt.
 *
 * @details  This function will fetch the conversion result from the ADC, convert the value into
 *           percentage and send it to peer.
 */
void saadc_event_handler(nrf_drv_saadc_evt_t const * p_event)
{

//NRF_LOG_INFO("adc event");

    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {

        ble_gatts_rw_authorize_reply_params_t auth_reply;
        nrf_saadc_value_t                     adc_result;
        uint16_t                              mux_lvl_in_milli_volts;
        uint16_t                              batt_lvl_in_milli_volts;
        uint8_t                               percentage_batt_lvl;
        uint32_t                              err_code;
        

        adc_result = p_event->data.done.p_buffer[0];

        err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 1);
        APP_ERROR_CHECK(err_code);

        mux_lvl_in_milli_volts = ADC_RESULT_IN_MILLI_VOLTS(adc_result);
        batt_lvl_in_milli_volts = MUX_RESULT_IN_BATT_VOLTAGE(mux_lvl_in_milli_volts);

        //Vmax=4000
        //Vmin=3400
                                                          //Vmin       //Vmax-Vmin
        percentage_batt_lvl = (((batt_lvl_in_milli_volts - 3400) * 100) / 600);

        NRF_LOG_INFO("mux mvolts: %d, batt mvolts: %d, batt percentage: %d", mux_lvl_in_milli_volts, batt_lvl_in_milli_volts, percentage_batt_lvl );

       
        if (saadc_return_value_handle == saadc_p_bas->battery_level_handles.value_handle ){

                memset(&auth_reply, 0, sizeof(auth_reply));
  
                auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                auth_reply.params.read.gatt_status=BLE_GATT_STATUS_SUCCESS;
                auth_reply.params.read.len = sizeof(uint8_t);
                //auth_reply.params.read.offset = 0;
                auth_reply.params.read.update = 1;
                auth_reply.params.read.p_data = &percentage_batt_lvl;
        
                err_code = sd_ble_gatts_rw_authorize_reply(saadc_return_conn_handle, &auth_reply);
                APP_ERROR_CHECK(err_code);
        }
        if (saadc_return_value_handle == saadc_p_bas->custom_battery_level_handles.value_handle ){

                char updatestring[20] = {0};
                //Update custom value
                snprintf(updatestring, sizeof(updatestring), "%03d%%; %d%d", percentage_batt_lvl, saadc_p_bas->battery_charging, saadc_p_bas->battery_charging_done);
                memset(&auth_reply, 0, sizeof(auth_reply));

                auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                auth_reply.params.read.gatt_status=BLE_GATT_STATUS_SUCCESS;
                auth_reply.params.read.len = 8;
                //auth_reply.params.read.offset = 0;
                auth_reply.params.read.update = 1;
                auth_reply.params.read.p_data = updatestring;
    
                err_code = sd_ble_gatts_rw_authorize_reply(saadc_return_conn_handle, &auth_reply);
                APP_ERROR_CHECK(err_code);
        
        }

        //un-initialize saadc to save power, high power usage! 2mA+
        nrf_drv_saadc_uninit();

        twi_write_register_data(0x28, 0x00); //write "Multiplexer disabled" to CNFG_CHG_I[7:0] - MUX_SEL


    }
}

void saadc_start()
{
    //initialize SAADC
    ret_code_t err_code = nrf_drv_saadc_init(NULL, saadc_event_handler);
    APP_ERROR_CHECK(err_code);

    nrf_saadc_channel_config_t config =
    NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(AMUX_ADC);
    err_code = nrf_drv_saadc_channel_init(0, &config);
    APP_ERROR_CHECK(err_code);


    err_code = nrf_drv_saadc_buffer_convert(&adc_buf[0], 1);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(&adc_buf[1], 1);
    APP_ERROR_CHECK(err_code);


    //sample SAADC
    err_code = nrf_drv_saadc_sample();
    APP_ERROR_CHECK(err_code);
}


static void adc_read_timeout_handler(void * p_context)
{
    saadc_start();
    NRF_LOG_INFO("ADC read timeout");

}




static void on_read_request(ble_bas_t * p_bas, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_read_t const * p_evt_read = &p_ble_evt->evt.gatts_evt.params.authorize_request.request.read;
    


    if ( (p_evt_read->handle == p_bas->battery_level_handles.value_handle) )
    {
        twi_write_register_data(0x28, 0x03); //write "BATT pin voltage" to CNFG_CHG_I[7:0] - MUX_SEL
            
        ret_code_t err_code;

        err_code = app_timer_start(m_timer_adc_read,
                                   APP_TIMER_TICKS(100),
                                   NULL);


        saadc_return_conn_handle  = p_ble_evt->evt.common_evt.conn_handle;
        saadc_return_value_handle = p_bas->battery_level_handles.value_handle;
        saadc_p_bas = p_bas;
        //saadc_start();

    }
    if ( (p_evt_read->handle == p_bas->custom_battery_level_handles.value_handle) )
    {
        twi_write_register_data(0x28, 0x03); //write "BATT pin voltage" to CNFG_CHG_I[7:0] - MUX_SEL
    
        saadc_return_conn_handle  = p_ble_evt->evt.common_evt.conn_handle;
        saadc_return_value_handle = p_bas->custom_battery_level_handles.value_handle;
        saadc_p_bas = p_bas;
        //saadc_start();
    }

}




void ble_bas_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    if ((p_context == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    ble_bas_t * p_bas = (ble_bas_t *)p_context;

    switch (p_ble_evt->header.evt_id)
    {

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
            if ( p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_READ  )
            {
                on_read_request(p_bas, p_ble_evt);
            }
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for adding the Battery Level characteristic.
 *
 * @param[in]   p_bas        Battery Service structure.
 * @param[in]   p_bas_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static ret_code_t battery_level_char_add(ble_bas_t * p_bas, const ble_bas_init_t * p_bas_init)
{
    ret_code_t             err_code;
    ble_add_char_params_t  add_char_params;
    ble_add_descr_params_t add_descr_params;
    uint8_t                initial_battery_level;
    ble_add_char_user_desc_t  user_descriptor;

    // Battery measurement characteristic
    initial_battery_level = 1;

    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char battery_measurement_desc[]      = "Battery Measurement (percentage)";
    user_descriptor.p_char_user_desc  = (uint8_t *) battery_measurement_desc;
    user_descriptor.max_size          = strlen(battery_measurement_desc);
    user_descriptor.size              = strlen(battery_measurement_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = BLE_UUID_BATTERY_LEVEL_CHAR;
    add_char_params.max_len           = sizeof(uint8_t);
    add_char_params.init_len          = sizeof(uint8_t);
    add_char_params.p_init_value      = &initial_battery_level;
    add_char_params.char_props.read   = 1;
    add_char_params.cccd_write_access = p_bas_init->bl_cccd_wr_sec;
    add_char_params.read_access       = p_bas_init->bl_rd_sec;
    add_char_params.p_user_descr      = &user_descriptor;
    add_char_params.is_defered_read   = 1;


    err_code = characteristic_add(p_bas->service_handle,
                                  &add_char_params,
                                  &(p_bas->battery_level_handles));
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Custom battery measurement characteristic.
    memset(&user_descriptor, 0, sizeof(user_descriptor));
    static char custom_battery_measurement_desc[]   = "Battery measurement (percentage) plus status (charging/charged)";
    user_descriptor.p_char_user_desc  = (uint8_t *) custom_battery_measurement_desc;
    user_descriptor.max_size          = strlen(custom_battery_measurement_desc);
    user_descriptor.size              = strlen(custom_battery_measurement_desc);
    user_descriptor.read_access       = 1;

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = BAS_UUID_CUSTOM_BATTERY;
    add_char_params.uuid_type         = p_bas->uuid_type;
    add_char_params.char_props.read   = 1;
    add_char_params.p_user_descr      = &user_descriptor;
    add_char_params.read_access       = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;
    add_char_params.is_value_user     = 0;
    add_char_params.is_defered_read   = 1;

    static char custom_battery_measurement_value[]           = "100%; 00";
    // Initial value

    add_char_params.max_len         = 8;
    add_char_params.init_len        = 8;
    add_char_params.p_init_value    = custom_battery_measurement_value;

    err_code = characteristic_add(p_bas->service_handle,
                                  &add_char_params,
                                  &p_bas->custom_battery_level_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }


    return NRF_SUCCESS;

}


ret_code_t ble_bas_init(ble_bas_t * p_bas, const ble_bas_init_t * p_bas_init)
{
    if (p_bas == NULL || p_bas_init == NULL)
    {
        return NRF_ERROR_NULL;
    }

    ret_code_t err_code;
    ble_uuid_t ble_uuid;

    //initialize timers
    err_code = app_timer_create(&m_timer_adc_read,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                adc_read_timeout_handler);
    APP_ERROR_CHECK(err_code);


    // Initialize service structure
    p_bas->evt_handler               = p_bas_init->evt_handler;
    p_bas->is_notification_supported = p_bas_init->support_notification;
    p_bas->battery_charging          = 0;
    p_bas->battery_charging_done     = 0;


    // Add service
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_BATTERY_SERVICE);

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_bas->service_handle);
    VERIFY_SUCCESS(err_code);

    //Add UUID for custom battery service
    ble_uuid128_t base_uuid = {MASTER_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_bas->uuid_type);
    VERIFY_SUCCESS(err_code);


    // Add battery level characteristics
    err_code = battery_level_char_add(p_bas, p_bas_init);


    return err_code;
}





ret_code_t ble_bas_battery_level_update_flags(uint16_t    conn_handle, ble_bas_t * p_bas,
                                              uint8_t     charging,
                                              uint8_t     charging_done)
{
    if (p_bas == NULL)
    {
        return NRF_ERROR_NULL;
    }

    ble_gatts_value_t  gatts_custom_value;
    char updatestring[20] = {0};

    p_bas->battery_charging      = charging;
    p_bas->battery_charging_done = charging_done;

    return NRF_SUCCESS;
}



#endif // NRF_MODULE_ENABLED(BLE_BAS)
