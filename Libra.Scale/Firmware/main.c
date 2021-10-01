/**
 * Copyright (c) 2015 - 2018, Nordic Semiconductor ASA
 *
 *    SDK 15.2.0
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdint.h>
#include <string.h>

#include "nrf_dfu_ble_svci_bond_sharing.h"
#include "nrf_svci_async_function.h"
#include "nrf_svci_async_handler.h"

#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_err.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"

#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "boards.h"
#include "app_timer.h"
#include "app_button.h"
#include "ble_bas.h"
#include "ble_dis.h"
#include "ble_sms.h"
#include "ble_lgs.h"
#include "ble_cfs.h"
#include "ble_dfu.h"

#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_delay.h"
#include "nrf_power.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_bootloader_info.h"

#include "twi_reader.h"
#include "flash_reader.h"
#include "spi_reader.h"


//GPIO pins
#define LED_BLE                         0                       /**< Led will blink to BLE activity */
#define LED_GPIO_CHARGING               3                       /**< GPIO counterpart of LED_CHARGING_ADDR */
#define LED_GPIO_BATTERY_FULL           1                       /**< GPIO counterpart of LED_BATTERY_FULL_ADDR */

#define LED_CHARGING_ADDR               0x40                    /**< Red LED - will turn on when charger connected turn off when battery full or charger disconnected */
#define LED_CHARGING_ENABLE             0b11011100              /**< CNFG_SNK2_A 0xc0 - 0xdf brightness control */
#define LED_CHARGING_DISABLE            0b00000000              

#define LED_BATTERY_FULL_ADDR           0x42                    /**< Green LED - will turn on when battery is full and turn off when charger disconnected */
#define LED_BATTERY_FULL_ENABLE         0b11011100              /**< CNFG_SNK1_A 0xc0 - 0xdf brightness control */
#define LED_BATTERY_FULL_DISABLE        0b00000000              


#define BUTTON_BATT_DETECTED            22                      /**< GPIO pin that detects battery charging */
#define BUTTON_TARE                     14                      /**< Tare button, unused for now */
#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(200)    /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */

#define HX711_ADC_RATE                  13
#define PMLDO                           24
#define POKLDO                          23
#define NIRQ                            28    //unused for now, button?
#define APOWER                          4     //TODO set gpio to 1 only when we need HX711 or blue LED, probably not needed

#define PIN_LOAD                        20
#define PIN_TXD                         6
#define PIN_RXD                         8
#define PIN_NRTS                        5
#define PIN_NCTS                        7


//Keys used for generating passwords
#define RESET_KEY                       "Us6Ek^eA0n![?[h33='N"    //len=20
#define SUDO_KEY                        "B9_ Dy^^;0<60$&852)S"    //len=20

//Strings passed to device information service
#define MANUFACTURER_NAME               "IJS"                                   
#define MODEL_NUMBER                    "Scale v4.0"
#define FW_REVISION                     "v1.0.0"
#define HW_REVISION                     "v4.0"

#define APP_BLE_OBSERVER_PRIO           3                                       /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG            1                                       /**< A tag identifying the SoftDevice BLE configuration. */

#define APP_ADV_INTERVAL                400                                     /**< The advertising interval (in units of 0.625 ms; this value corresponds to 40 ms). */
#define APP_ADV_DURATION                BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED   /**< The advertising time-out (in units of seconds). When set to 0, we will never time out. */
#define CUSTOM_APP_BLE_TX_POWER         -20                                     /**< TX power in dbm */



#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)        /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)        /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                   0                                       /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)         /**< Connection supervisory time-out (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(20000)                  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (15 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(5000)                   /**< Time between each call to sd_ble_gap_conn_param_update after the first call (5 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                       /**< Number of attempts before giving up the connection parameter negotiation. */


#define SETTING_MARGIN_REFRESH_MIN      62                  /**< Values used to clamp min-max received from ble */
#define SETTING_MARGIN_REFRESH_MAX      5000
#define SETTING_MARGIN_NOTIFY_MIN       200                      
#define SETTING_MARGIN_NOTIFY_MAX       5000
#define SETTING_MARGIN_TIMEOUT_MIN      1
#define SETTING_MARGIN_TIMEOUT_MAX      60

#define SCALE_NUMBER_OF_AVG_INPUTS      5                   /** How many values to use for calculating avg */
#define SCALE_NOTIFYING_MARGIN          80                  /** Required deviation (of the averaged raw HX711 read value) before new value is notified */

#define DEAD_BEEF                       0xDEADBEEF          /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
                                                //TODO
#define USER_LEVEL_USER                0       //0 for default operation, -1 for debug
#define USER_LEVEL_SUDO                1       //1 for default operation, -1 for debug

#define CHARGER_UPDATE_INTERVAL         APP_TIMER_TICKS(1000)                   /**< Time charger stat readings when power connected */
#define LED_BLE_OFF_TIMEOUT             APP_TIMER_TICKS(20)                     /**< Duration of blue led blinks */

//#define LED_RED_GREEN_GPIO_ENABLED

APP_TIMER_DEF(m_timer_refresh_time);        //setting_refresh_time      - Time between reading scale from SPI
APP_TIMER_DEF(m_timer_refresh_notify);      //setting_refresh_notify    - Time between sending scale values as BLE notification
APP_TIMER_DEF(m_timer_charger);             //fixed time                - Time between charge controller I2C stats readings only while charger connected
APP_TIMER_DEF(m_timer_power_timeout);       //setting_timeout           - Time until scale powers off
APP_TIMER_DEF(m_timer_led_off);             //fixed time                - Time until BLE led turns off


BLE_SMS_DEF(m_sms);                                                             /**< Scale measurement service instance. */
BLE_LGS_DEF(m_lgs);                                                             /**< Login service instance. */
BLE_CFS_DEF(m_cfs);                                                             /**< Configuration service instance. */
BLE_BAS_DEF(m_bas);                                                             /**< Battery service instance. */

NRF_BLE_GATT_DEF(m_gatt);                                                       /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                         /**< Context for the Queued Write module.*/


static uint8_t password_reset[20];                                              /** Password used for resetting the password to default */
static uint8_t password_sudo[20];                                               /** Sudo password */


static uint32_t setting_refresh_time;                                           /** Refresh time setting */
static uint32_t setting_refresh_notify;                                       /** Refresh time notify setting */
static uint32_t setting_timeout;                                                /** Timeout setting */
static int32_t  setting_correction_factor;                                      /** Correction factor */
static uint32_t setting_gain_rate;                                              /** Gain and rate for spi adc */
static char     setting_password[40];                                           /** User password */      
static char     setting_name[40];                                               /** Device name */

static uint8_t  user_level = 0;                                                 /** Login level 0=no login 1=user 2=sudo */
static bool     m_fstorage_pending = false;
static bool     m_is_notification_enabled = false;


static bool     m_led_charging = false;                                         /** Current led status  */
static bool     m_led_battery_full = false;
static bool     m_led_charging_prev_state = false;                              /** For toggling R and G led during blue led blinks  */
static bool     m_led_battery_full_prev_state = false;


static int32_t scale_input_array[SCALE_NUMBER_OF_AVG_INPUTS] = {0};                       /** Array of scale readings used for avg */
static int     scale_index                        = 0;

static int32_t scale_current_input                = 0;                         /** Current reading read from SPI */
static int32_t scale_current_avg                  = 0;                         /** Averaged scale value from last few readings */
static int32_t scale_current_tare                 = 0;                         /** Current tare same units as 'scale_current_avg' */
static int32_t scale_last_sent                    = 0;                         
static bool    tare_wakeup                        = false;

static char    scale_notify_string[7];                                       /** String that is sent out, always 6 chars, example: 00555g */

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;                        /**< Handle of the current connection. */

static uint8_t  m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;                  /**< Advertising handle used to identify an advertising set. */
static uint8_t  m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];                   /**< Buffer for storing an encoded advertising set. */
static uint8_t  m_enc_scan_response_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX];        /**< Buffer for storing an encoded scan data. */






/**@brief Struct that contains pointers to the encoded advertising data. */
static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_enc_advdata,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = m_enc_scan_response_data,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX

    }
};

/**@brief Function for assert macro callback.
 * 
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
 //TODO
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);


    NRF_LOG_WARNING("System reset");
    NVIC_SystemReset();

}



void gpio_init()
{
    int i=0;
    for (i=0;i<31;i++)
        nrf_gpio_cfg_default(i);

    //Leds
    #ifdef LED_RED_GREEN_GPIO_ENABLED
    nrf_gpio_cfg_output(LED_GPIO_CHARGING);
    nrf_gpio_pin_set(LED_GPIO_CHARGING);
    nrf_gpio_cfg_output(LED_GPIO_BATTERY_FULL);
    nrf_gpio_pin_set(LED_GPIO_BATTERY_FULL);
    #endif

    nrf_gpio_cfg_output(LED_BLE);
    nrf_gpio_pin_clear(LED_BLE);

    //Other pins
    //Note: Each input pin draws ~25uA
    //nrf_gpio_cfg_input(POKLDO, NRF_GPIO_PIN_NOPULL);



    nrf_gpio_cfg_default(POKLDO);
    nrf_gpio_cfg_default(PIN_TXD);
    nrf_gpio_cfg_default(PIN_RXD);
    nrf_gpio_cfg_default(PIN_NRTS);
    nrf_gpio_cfg_default(PIN_NCTS);
    nrf_gpio_cfg_default(PIN_LOAD);


  

    nrf_gpio_cfg_output(HX711_ADC_RATE);
    nrf_gpio_cfg_output(PMLDO);
    nrf_gpio_cfg_output(APOWER);
    


    nrf_gpio_pin_clear(APOWER); //1 = enabled SYS power on MAX77734
    nrf_gpio_pin_set(PMLDO);  //1 = low power mode, 0 = normal
    nrf_gpio_pin_set(HX711_ADC_RATE); //0 = 10hz

    // bit0 and bit1 -> select gain 0 = 32;   1=64;   2=128
    // bit2          -> select rate 0 = 10hz; 1=80hz

    if(setting_gain_rate & 4)
    {
        nrf_gpio_pin_set(HX711_ADC_RATE); //1 = 80hz
        NRF_LOG_INFO("SPI ADC Rate: 80");
    }
    else
    {
        nrf_gpio_pin_clear(HX711_ADC_RATE); //0 = 10hz
        NRF_LOG_INFO("SPI ADC Rate: 10");
    }

}

void led_control_red_green(bool led_red, bool led_green)
{

  //if prev_state same as next state, do nothing
  if ((m_led_charging == led_red) && (m_led_battery_full == led_green))
    return;



  if(led_red)
  {
      NRF_LOG_INFO("Charging led ON");
      m_led_charging=true;
      twi_write_register_data(LED_CHARGING_ADDR, LED_CHARGING_ENABLE);
      #ifdef LED_RED_GREEN_GPIO_ENABLED
      nrf_gpio_pin_clear(LED_GPIO_CHARGING);
      #endif
  }
  else
  {
      NRF_LOG_INFO("Charging led OFF");
      m_led_charging=false;
      twi_write_register_data(LED_CHARGING_ADDR, LED_CHARGING_DISABLE);
      #ifdef LED_RED_GREEN_GPIO_ENABLED
      nrf_gpio_pin_set(LED_GPIO_CHARGING);
      #endif
  }

  if(led_green)
  {
      NRF_LOG_INFO("Battery full led ON");
      m_led_battery_full=true;
      twi_write_register_data(LED_BATTERY_FULL_ADDR, LED_BATTERY_FULL_ENABLE);
      #ifdef LED_RED_GREEN_GPIO_ENABLED
      nrf_gpio_pin_clear(LED_GPIO_CHARGING);
      #endif
  }
  else
  {
      NRF_LOG_INFO("Battery full led OFF");
      m_led_battery_full=false;
      twi_write_register_data(LED_BATTERY_FULL_ADDR, LED_BATTERY_FULL_DISABLE);
      #ifdef LED_RED_GREEN_GPIO_ENABLED
      nrf_gpio_pin_set(LED_GPIO_BATTERY_FULL);
      #endif
  }

}



/* -------------------------------
 *              DFU
 * -------------------------------
 */

/**@brief Handler for shutdown preparation.
 *
 * @details During shutdown procedures, this function will be called at a 1 second interval
 *          untill the function returns true. When the function returns true, it means that the
 *          app is ready to reset to DFU mode.
 *
 * @param[in]   event   Power manager event.
 *
 * @retval  True if shutdown is allowed by this power manager handler, otherwise false.
 */
static bool app_shutdown_handler(nrf_pwr_mgmt_evt_t event)
{
    switch (event)
    {
        case NRF_PWR_MGMT_EVT_PREPARE_DFU:
        NRF_LOG_INFO("Power management wants to reset to DFU mode.");
        if (m_fstorage_pending)
        {
            return false;
        }
        else //Prepare for reset to bootloader
        {
            ret_code_t err_code = app_timer_stop_all(); //stop all timers as system is resetting to bootloader
            APP_ERROR_CHECK(err_code);
            nrf_gpio_pin_clear(LED_BLE); //turn off BLE led in case ble led timer was stopped while running
            
            err_code = app_button_disable();
            APP_ERROR_CHECK(err_code);
            
            fstorage_uninit();      //un-init flash


        }
        break;

        default:
            // Unused power management module events:
            //      NRF_PWR_MGMT_EVT_PREPARE_SYSOFF
            //      NRF_PWR_MGMT_EVT_PREPARE_WAKEUP
            //      NRF_PWR_MGMT_EVT_PREPARE_RESET
            return true;
    }

    NRF_LOG_INFO("Power management allowed to reset to DFU mode.");
    return true;
}
/**@brief Register application shutdown handler with priority 0.
 */
NRF_PWR_MGMT_HANDLER_REGISTER(app_shutdown_handler, 0);

static void buttonless_dfu_sdh_state_observer(nrf_sdh_state_evt_t state, void * p_context)
{
    if (state == NRF_SDH_EVT_STATE_DISABLED)
    {
        // Softdevice was disabled before going into reset. Inform bootloader to skip CRC on next boot.
        nrf_power_gpregret2_set(BOOTLOADER_DFU_SKIP_CRC);

        //Go to system off.
        nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
    }
}

/* nrf_sdh state observer. */
NRF_SDH_STATE_OBSERVER(m_buttonless_dfu_state_obs, 0) =
{
    .handler = buttonless_dfu_sdh_state_observer,
};


// YOUR_JOB: Update this code if you want to do anything given a DFU event (optional).
/**@brief Function for handling dfu events from the Buttonless Secure DFU service
 *
 * @param[in]   event   Event from the Buttonless Secure DFU service.
 */
static void ble_dfu_evt_handler(ble_dfu_buttonless_evt_type_t event)
{
    switch (event)
    {
        case BLE_DFU_EVT_BOOTLOADER_ENTER_PREPARE:
        {
            NRF_LOG_INFO("Device is preparing to enter bootloader mode.");
            
            if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                ret_code_t err_code = sd_ble_gap_disconnect(m_conn_handle,
                                 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                APP_ERROR_CHECK(err_code);

            }
            break;
        }

        case BLE_DFU_EVT_BOOTLOADER_ENTER:
            // YOUR_JOB: Write app-specific unwritten data to FLASH, control finalization of this
            //           by delaying reset by reporting false in app_shutdown_handler
            NRF_LOG_INFO("Device will enter bootloader mode.");
            break;

        case BLE_DFU_EVT_BOOTLOADER_ENTER_FAILED:
            NRF_LOG_ERROR("Request to enter bootloader mode failed asynchroneously.");
            // YOUR_JOB: Take corrective measures to resolve the issue
            //           like calling APP_ERROR_CHECK to reset the device.
            break;

        case BLE_DFU_EVT_RESPONSE_SEND_ERROR:
            NRF_LOG_ERROR("Request to send a response to client failed.");
            // YOUR_JOB: Take corrective measures to resolve the issue
            //           like calling APP_ERROR_CHECK to reset the device.
            APP_ERROR_CHECK(false);
            break;

        default:
            NRF_LOG_ERROR("Unknown event from ble_dfu_buttonless.");
            break;
    }
}









/* -------------------------------
 *            Timers
 * -------------------------------
 */

//this timer repeatedly reads weight to calculate the average.
static void refresh_timeout_handler(void * p_context)
{

    //NRF_LOG_INFO("refresh timeout");
    scale_current_input = spi_read_data();
    //NRF_LOG_HEXDUMP_INFO(&scale_current_input, 4);
    if(scale_current_input == -1)
    {
        NRF_LOG_INFO("spi data -skipped-");
    }
    else
    {
        NRF_LOG_INFO("spi data %d",scale_current_input);
        if(scale_index>=SCALE_NUMBER_OF_AVG_INPUTS)
          scale_index = 0;

        scale_input_array[scale_index] = scale_current_input;
        scale_index++;

        int i;
        int sum=0;
        for(i=0;i<SCALE_NUMBER_OF_AVG_INPUTS;i++)
          sum += scale_input_array[i];

        scale_current_avg = sum/SCALE_NUMBER_OF_AVG_INPUTS;
    }

    //NRF_LOG_INFO("in:%x; avg:%x", scale_current_input, scale_current_avg );

    /*
    NRF_LOG_INFO("Avgd data: %d", scale_current_avg );
    NRF_LOG_INFO("SPI data: %d", scale_current_input );
    */

}


static void notify_timeout_handler(void * p_context)
{

    //NRF_LOG_INFO("notify timeout");

    if((scale_current_avg > scale_last_sent+SCALE_NOTIFYING_MARGIN) || (scale_current_avg < scale_last_sent-SCALE_NOTIFYING_MARGIN) || tare_wakeup)
    {
        tare_wakeup = false;
        //weight has changed, restart power timeout.

        //reset power timeout as weight has changed
        if(setting_timeout!=0)
        {
            ret_code_t err_code = app_timer_stop(m_timer_power_timeout);
            APP_ERROR_CHECK(err_code);
            err_code = app_timer_start(m_timer_power_timeout,
                                       APP_TIMER_TICKS(setting_timeout*60*1000),
                                       NULL);
            APP_ERROR_CHECK(err_code);
        }
        scale_last_sent = scale_current_avg;

      //send notification
      int32_t scale_weight_in_grams =   (((int)scale_current_avg - (int)scale_current_tare )*100) / (int)setting_correction_factor  ;
      NRF_LOG_INFO("Sending %d", scale_weight_in_grams);



      if(scale_weight_in_grams<0){
      scale_weight_in_grams*=-1;
        snprintf(scale_notify_string, 7, "-%04dg",  scale_weight_in_grams);
        }
      else
        snprintf(scale_notify_string, 7, " %04dg",  scale_weight_in_grams);
    
    if(user_level>USER_LEVEL_USER) //only send if user logged in
    {
        ble_sms_send_measurement(m_conn_handle, &m_sms, scale_notify_string);
    }
    else
        NRF_LOG_INFO("User not logged in.");




    }
    //else
      //NRF_LOG_INFO("Inside margins, not resetting poweroff timeout");


}




static void charger_timeout_handler(void * p_context)
{

    NRF_LOG_INFO("charger timeout");
    volatile int data_b = twi_read_register_data(0x03); //STAT_CHG_B
    //NRF_LOG_INFO("CHG register - charging is happening: %x", data_b & 0b00000010);

    if(data_b & 0b00000010)
    {
        NRF_LOG_INFO("MAX says battery is charging");

        led_control_red_green(1,0);

        ble_bas_battery_level_update_flags(m_conn_handle, &m_bas, 1,0);
    }
    else
    {
        NRF_LOG_INFO("MAX says battery is not charging");
        
        led_control_red_green(0,1);

        ble_bas_battery_level_update_flags(m_conn_handle, &m_bas, 0,1);
    }


}

static void power_timeout_handler(void * p_context)
{

    NRF_LOG_INFO("Power off: Powering off scale and disconnecting user");
    ret_code_t err_code;
    //turn off HX711
    spi_power_off();

    //stop timers as user will be disconnected
    err_code = app_timer_stop(m_timer_refresh_time);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_stop(m_timer_refresh_notify);
    APP_ERROR_CHECK(err_code);


    //TODO set this pin for power off mode
    /*
    nrf_gpio_pin_set(PMLDO);
    */

    //disconnect user
    /*
    err_code = sd_ble_gap_disconnect(m_conn_handle,
                     BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    */
}

static void led_ble_timeout_handler(void * p_context)
{ // blue led timer
    nrf_gpio_pin_clear(LED_BLE);  //turn off BLE led
    led_control_red_green(m_led_charging_prev_state, m_led_battery_full_prev_state);  //restore red and green to previous state

}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    // Initialize timer module, making it use the scheduler
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    //Create timers
    err_code = app_timer_create(&m_timer_refresh_time,
                                APP_TIMER_MODE_REPEATED,
                                refresh_timeout_handler);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_create(&m_timer_refresh_notify,
                                APP_TIMER_MODE_REPEATED,
                                notify_timeout_handler);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_create(&m_timer_charger,
                                APP_TIMER_MODE_REPEATED,
                                charger_timeout_handler);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_create(&m_timer_power_timeout,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                power_timeout_handler);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_create(&m_timer_led_off,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                led_ble_timeout_handler);
    APP_ERROR_CHECK(err_code);

}



/* -------------------------------
 *            Buttons    
 * -------------------------------
 */



/**@brief Function for handling events from the button handler module.
 *
 * @param[in] pin_no        The pin that the event applies to.
 * @param[in] button_action The button action (press/release).
 */
static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{
    ret_code_t err_code;

    switch (pin_no)
    {
        case BUTTON_BATT_DETECTED:
            
            if (button_action){
                NRF_LOG_INFO("charger connected");
                //start charger timer for reading i2c charge stats (charging, fully charged)
                err_code = app_timer_start(m_timer_charger,
                                           CHARGER_UPDATE_INTERVAL,
                                           NULL);
                APP_ERROR_CHECK(err_code);
                
                //turn on charging led, m_timer_charger will figure out the rest

                led_control_red_green(1,0);

            }
            else  //charger not connected
            {
                NRF_LOG_INFO("charger disconnected");
                ble_bas_battery_level_update_flags(m_conn_handle, &m_bas, 0,0); //remove both flags
                err_code = app_timer_stop(m_timer_charger);
                APP_ERROR_CHECK(err_code);
                
                //turn off both leds
                led_control_red_green(0,0);

            }
            break;

        default:
            APP_ERROR_HANDLER(pin_no);
            break;
    }
}


/**@brief Function for button initialization.
 *
 * @details Sets the gpio pins
 */
static void buttons_init(void)
{
    ret_code_t err_code;


    //The array must be static because a pointer to it will be saved in the button handler module.
    static app_button_cfg_t buttons[] =
    {
        {BUTTON_BATT_DETECTED, APP_BUTTON_ACTIVE_HIGH, NRF_GPIO_PIN_NOPULL, button_event_handler},
    };

    err_code = app_button_init(buttons, ARRAY_SIZE(buttons),
                               BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);

    //enable button event
    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);


}

// Fills a 6 byte array with Device ID in LITTLE ENDIAN.
// Remaining 2 DEVICEID most significant bytes are ignored.
void get_serial_number(uint8_t serial_number[6])
{
    serial_number[5] = (NRF_FICR->DEVICEID[0] & 0x000000FF);
    serial_number[4] = (NRF_FICR->DEVICEID[0] & 0x0000FF00) >> 8;
    serial_number[3] = (NRF_FICR->DEVICEID[0] & 0x00FF0000) >> 16;
    serial_number[2] = (NRF_FICR->DEVICEID[0] & 0xFF000000) >> 24;
    serial_number[1] = (NRF_FICR->DEVICEID[1] & 0x000000FF);
    serial_number[0] = (NRF_FICR->DEVICEID[1] & 0x0000FF00) >> 8;

}


/**@brief Function for the GAP initialization.
 *
 * @details This Function generates password reset password and sudo password and saves them to their variable.
 */
static void passwords_init(void)
{
    
    uint8_t reset_key[20] = RESET_KEY;
    uint8_t sudo_key[20] = SUDO_KEY;
    uint8_t serial_number[6];
    get_serial_number(serial_number);
    ble_gap_addr_t ble_mac_addr;
    sd_ble_gap_addr_get(&ble_mac_addr);

    //mac big endian
    //serial little endian

    password_reset[0]  = ble_mac_addr.addr[0] ^ ble_mac_addr.addr[2] ^ reset_key[0];
    password_reset[1]  = ble_mac_addr.addr[1] ^ ble_mac_addr.addr[3] ^ reset_key[1];
    password_reset[2]  = ble_mac_addr.addr[0] ^ reset_key[2];
    password_reset[3]  = ble_mac_addr.addr[1] ^ reset_key[3];
    password_reset[4]  = ble_mac_addr.addr[2] ^ reset_key[4];
    password_reset[5]  = ble_mac_addr.addr[3] ^ reset_key[5];
    password_reset[6]  = ble_mac_addr.addr[4] ^ reset_key[6];
    password_reset[7]  = ble_mac_addr.addr[5] ^ reset_key[7];
    password_reset[8]  = serial_number[5] ^ reset_key[8];
    password_reset[9]  = serial_number[4] ^ reset_key[9];
    password_reset[10] = serial_number[3] ^ reset_key[10];
    password_reset[11] = serial_number[2] ^ reset_key[11];
    password_reset[12] = serial_number[1] ^ reset_key[12];
    password_reset[13] = serial_number[0] ^ reset_key[13];
    password_reset[14] = ble_mac_addr.addr[0] ^ serial_number[5] ^ reset_key[14];
    password_reset[15] = ble_mac_addr.addr[1] ^ serial_number[4] ^ reset_key[15];
    password_reset[16] = ble_mac_addr.addr[2] ^ serial_number[3] ^ reset_key[16];
    password_reset[17] = ble_mac_addr.addr[3] ^ serial_number[2] ^ reset_key[17];
    password_reset[18] = ble_mac_addr.addr[4] ^ serial_number[1] ^ reset_key[18];
    password_reset[19] = ble_mac_addr.addr[5] ^ serial_number[0] ^ reset_key[19];

    password_sudo[0]  = ble_mac_addr.addr[0] ^ ble_mac_addr.addr[2] ^ sudo_key[0];
    password_sudo[1]  = ble_mac_addr.addr[1] ^ ble_mac_addr.addr[3] ^ sudo_key[1];
    password_sudo[2]  = ble_mac_addr.addr[0] ^ sudo_key[2];
    password_sudo[3]  = ble_mac_addr.addr[1] ^ sudo_key[3];
    password_sudo[4]  = ble_mac_addr.addr[2] ^ sudo_key[4];
    password_sudo[5]  = ble_mac_addr.addr[3] ^ sudo_key[5];
    password_sudo[6]  = ble_mac_addr.addr[4] ^ sudo_key[6];
    password_sudo[7]  = ble_mac_addr.addr[5] ^ sudo_key[7];
    password_sudo[8]  = serial_number[5] ^ sudo_key[8];
    password_sudo[9]  = serial_number[4] ^ sudo_key[9];
    password_sudo[10] = serial_number[3] ^ sudo_key[10];
    password_sudo[11] = serial_number[2] ^ sudo_key[11];
    password_sudo[12] = serial_number[1] ^ sudo_key[12];
    password_sudo[13] = serial_number[0] ^ sudo_key[13];
    password_sudo[14] = ble_mac_addr.addr[0] ^ serial_number[5] ^ sudo_key[14];
    password_sudo[15] = ble_mac_addr.addr[1] ^ serial_number[4] ^ sudo_key[15];
    password_sudo[16] = ble_mac_addr.addr[2] ^ serial_number[3] ^ sudo_key[16];
    password_sudo[17] = ble_mac_addr.addr[3] ^ serial_number[2] ^ sudo_key[17];
    password_sudo[18] = ble_mac_addr.addr[4] ^ serial_number[1] ^ sudo_key[18];
    password_sudo[19] = ble_mac_addr.addr[5] ^ serial_number[0] ^ sudo_key[19];

/*
    NRF_LOG_INFO("Mac: ");
    NRF_LOG_HEXDUMP_INFO(ble_mac_addr.addr, 6);

    NRF_LOG_INFO("Serial: ");
    NRF_LOG_HEXDUMP_INFO(serial_number, 6);

    NRF_LOG_INFO("Reset pass: ");
    NRF_LOG_HEXDUMP_INFO(password_reset, 20);

    NRF_LOG_INFO("Sudo pass: ");
    NRF_LOG_HEXDUMP_INFO(password_sudo, 20);
*/

}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;
    ble_gap_addr_t          ble_mac_addr;   /** BLE MAC address */

    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&sec_mode);


    sd_ble_gap_addr_get(&ble_mac_addr); //get MAC

    if(strcmp(setting_name, "Libra") == 0) //Default name => append MAC suffix
    { 
        snprintf(setting_name+5, sizeof(setting_name)-5, " %02x:%02x:%02x\0", ble_mac_addr.addr[2],ble_mac_addr.addr[1],ble_mac_addr.addr[0]);
    }
    

    //NRF_LOG_INFO("Name is '%s'",setting_name);
    //NRF_LOG_HEXDUMP_INFO(setting_name, 29);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          setting_name,
                                          strlen(setting_name));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_appearance_set(0x0C80);
    APP_ERROR_CHECK(err_code);


    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;
    
    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);


}


/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
    ret_code_t    err_code;
    ble_advdata_t advdata;
    //ble_advdata_t srdata;


    ble_uuid_t adv_uuids[] = 
    {
        {BLE_UUID_BATTERY_SERVICE,              BLE_UUID_TYPE_BLE},
        {BLE_UUID_DEVICE_INFORMATION_SERVICE,   BLE_UUID_TYPE_BLE},
    };

    ble_uuid_t adv_uuids_incomplete[] = 
    {
       {SMS_UUID_SERVICE, m_lgs.uuid_type},
    };

    // Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = true;
    advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    /** -- No need for scan response 
    memset(&srdata, 0, sizeof(srdata));
    srdata.uuids_more_available.uuid_cnt = sizeof(adv_uuids_incomplete) / sizeof(adv_uuids_incomplete[0]);
    srdata.uuids_more_available.p_uuids  = adv_uuids_incomplete;

    srdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    srdata.uuids_complete.p_uuids  = adv_uuids;
    */    

    err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
    APP_ERROR_CHECK(err_code);

    /**
    err_code = ble_advdata_encode(&srdata, m_adv_data.scan_rsp_data.p_data, &m_adv_data.scan_rsp_data.len);
    APP_ERROR_CHECK(err_code);
    */

    ble_gap_adv_params_t adv_params;

    // Set advertising parameters.
    memset(&adv_params, 0, sizeof(adv_params));

    adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;
    adv_params.duration        = APP_ADV_DURATION;
    adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.p_peer_addr     = NULL;
    adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
    adv_params.interval        = APP_ADV_INTERVAL;

    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &adv_params);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_adv_handle, CUSTOM_APP_BLE_TX_POWER);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}









/* ----------- login service ------------ */

/**@brief Function for handling write events to the password input characteristic.
 *
 * @param[in] p_lgs         Instance of login service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void password_input_handler(uint16_t conn_handle, ble_lgs_t * p_lgs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    ret_code_t err_code;

    NRF_LOG_INFO("Password input got: ");
    NRF_LOG_HEXDUMP_INFO(p_evt_write->data, p_evt_write->len);

    if(memcmp(p_evt_write->data, password_reset, p_evt_write->len)==0 && p_evt_write->len == 20)
    {
        NRF_LOG_INFO("Reset password to default and logged in as user");

        memset(&setting_password, 0, sizeof(setting_password));
        strncpy(setting_password, "0000", 4);

        m_fstorage_pending = true; //Write setting variables to flash before going to cpu idle.
        user_level=1;

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else if(memcmp(p_evt_write->data, password_sudo, p_evt_write->len)==0 && p_evt_write->len == 20)
    {
        NRF_LOG_INFO("Logged in as sudo");
        user_level=2;

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else if(memcmp(p_evt_write->data, setting_password, p_evt_write->len)==0 && p_evt_write->len == strlen(setting_password))
    {
        NRF_LOG_INFO("Logged in as user");
        user_level=1;

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else
    {
        NRF_LOG_INFO("No match.");
            
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=0x81;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

        //also disconnect user
        if(!user_level)
        {
            err_code = sd_ble_gap_disconnect(m_conn_handle,
                     BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
        }
    }

}


/**@brief Function for handling write events to the password change characteristic.
 *
 * @param[in] p_lgs         Instance of login service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void password_change_handler(uint16_t conn_handle, ble_lgs_t * p_lgs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    ret_code_t err_code;

    if(user_level>USER_LEVEL_USER)
    {
        NRF_LOG_INFO("Password changer got: ");
        NRF_LOG_HEXDUMP_INFO(p_evt_write->data, p_evt_write->len);

        //NRF_LOG_HEXDUMP_INFO(p_evt_write->data, p_evt_write->len+10);
        memset(&setting_password, 0, sizeof(setting_password)); //strncpy does not append 0
        strncpy(setting_password, p_evt_write->data, p_evt_write->len);   //cat appends \0 unlike cpy, max data length=20 limited by characteristic
        NRF_LOG_INFO("new password: %s",setting_password);

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }


}










/* ----------- configuration service ------------ */

/**@brief Function for handling write events to the tare characteristic.
 *
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void tare_handler(uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    ret_code_t err_code;

    if(user_level>USER_LEVEL_USER)
    {

        NRF_LOG_INFO("New tare: %d", scale_current_avg);
        scale_current_tare = scale_current_avg;
        tare_wakeup = true;

        //NRF_LOG_HEXDUMP_INFO(p_evt_write->data, p_evt_write->len);

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }

}
/**@brief Function for handling write events to power off characteristic.
 *
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void power_off_handler(uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    ret_code_t err_code;

    if(user_level>USER_LEVEL_USER)
    {
        NRF_LOG_INFO("Manual power off");
        
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

        power_timeout_handler(0); //trigger power off timer timeout handler

      
    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }


}
/**@brief Function for handling write events to the set refresh time characteristic.
 *  updates the refresh time setting variable and updates the timer.
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void set_refresh_time_handler(uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    uint16_t                              gatt_status = BLE_GATT_STATUS_SUCCESS;
    ret_code_t                            err_code;


    if(user_level>USER_LEVEL_USER)
    {
        //received ascii text
        char *datastring = strndup(p_evt_write->data, p_evt_write->len);
        int dataint      = atoi(datastring);
        if ( dataint > SETTING_MARGIN_REFRESH_MAX)
        {
            dataint = SETTING_MARGIN_REFRESH_MAX;
            gatt_status = 0x86;
        }
        if (dataint < SETTING_MARGIN_REFRESH_MIN )
        {
            dataint = SETTING_MARGIN_REFRESH_MIN;
            gatt_status = 0x87;
        }
        setting_refresh_time = dataint;

        //restart timer with new interval if timer is already running
        if (m_is_notification_enabled)  //timer should be started
        {
            err_code = app_timer_stop(m_timer_refresh_time);
            APP_ERROR_CHECK(err_code);
                       err_code = app_timer_start(m_timer_refresh_time,
                                                           APP_TIMER_TICKS(setting_refresh_time),
                                                           NULL);
            APP_ERROR_CHECK(err_code);

        }

        NRF_LOG_INFO("new scale sampling rate (refresh time): %d",dataint );

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status = gatt_status;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling write events to the set refresh time notify characteristic.
 *  updates the refresh time setting notify variable and updates the timer.
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void set_refresh_notify_handler(uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    uint16_t                              gatt_status = BLE_GATT_STATUS_SUCCESS;
    ret_code_t                            err_code;

    if(user_level>USER_LEVEL_USER)
    {
        //received ascii text
        char *datastring = strndup(p_evt_write->data, p_evt_write->len);
        int dataint      = atoi(datastring);
        if ( dataint > SETTING_MARGIN_NOTIFY_MAX)
        {
            dataint = SETTING_MARGIN_NOTIFY_MAX;
            gatt_status = 0x86;
        }
        if (dataint < SETTING_MARGIN_NOTIFY_MIN )
        {
            dataint = SETTING_MARGIN_NOTIFY_MIN;
            gatt_status = 0x87;
        }

        setting_refresh_notify = dataint;

        //restart notification timer with new interval if timer is already running

        if (m_is_notification_enabled)  //timer should be started
        {
            err_code = app_timer_stop(m_timer_refresh_notify);
            APP_ERROR_CHECK(err_code);
                       err_code = app_timer_start(m_timer_refresh_notify,
                                                           APP_TIMER_TICKS(setting_refresh_notify),
                                                           NULL);
            APP_ERROR_CHECK(err_code);

        }


        NRF_LOG_INFO("new notification interval (refresh notify): %d",dataint );

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=gatt_status;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }

}


/**@brief Function for handling write events to set turn off timeout (sleep) characteristic.
 *
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void set_turn_off_timeout_handler(uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    uint16_t                              gatt_status = BLE_GATT_STATUS_SUCCESS;
    ret_code_t                            err_code;

    if(user_level>USER_LEVEL_USER)
    {
        //received ascii text
        char *datastring = strndup(p_evt_write->data, p_evt_write->len);
        int dataint      = atoi(datastring);
        if ( dataint > SETTING_MARGIN_TIMEOUT_MAX)
        {
            dataint = SETTING_MARGIN_TIMEOUT_MAX;
            gatt_status = 0x86;
        }
        if (dataint < SETTING_MARGIN_TIMEOUT_MIN )
        {
            dataint = SETTING_MARGIN_TIMEOUT_MIN;
            gatt_status = 0x87;
        }
        setting_timeout = dataint;

        NRF_LOG_INFO("turn off timeout: %d", dataint);

        //restart timer with new timeout
        err_code = app_timer_stop(m_timer_power_timeout);
        APP_ERROR_CHECK(err_code);
        if(setting_timeout!=0)
        {
            err_code = app_timer_start(m_timer_power_timeout,
                                      APP_TIMER_TICKS(setting_timeout*60*1000),
                                      NULL);
            APP_ERROR_CHECK(err_code);
        }
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=gatt_status;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }


}
/**@brief Function for handling write events to the set correction characteristic.
 *
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void set_correction_handler(uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    ret_code_t err_code;
    NRF_LOG_INFO("correction handler");
    if(user_level>USER_LEVEL_SUDO)
    {

        //received int32
        if (p_evt_write->len < 4) //Do nothing if not a full int32
          return;

        int32_t dataint;
        memcpy(&dataint, p_evt_write->data, 4);
    
        setting_correction_factor = dataint;

        NRF_LOG_INFO("set correction: %d",dataint);

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }
   
}
/**@brief Function for handling write events to the set gain and rate characteristic.
 *
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void set_gain_rate_handler(uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    ret_code_t err_code;

    if(user_level>USER_LEVEL_SUDO)
    {

        uint8_t dataint;
        memcpy(&dataint, p_evt_write->data, 1);

        NRF_LOG_INFO("Gain rate got: %d", dataint);
        // bit0 and bit1 -> select gain 0 = 32;   1=64;   2=128
        // bit2          -> select rate 0 = 10hz; 1=80hz
        setting_gain_rate = dataint;

        //update rate
        if(setting_gain_rate & 4)
        {
            nrf_gpio_pin_clear(HX711_ADC_RATE); //1 = 80hz
            NRF_LOG_INFO("SPI ADC Rate: 80");
        }
        else
        {
            nrf_gpio_pin_set(HX711_ADC_RATE); //0 = 10hz
            NRF_LOG_INFO("SPI ADC Rate: 10");
        }
        //update gain
        spi_set_gain(setting_gain_rate);

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }

    
}

/**@brief Function for handling write events to the change name characteristic.
 *
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void change_name_handler(uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    ret_code_t err_code;

    if(user_level>USER_LEVEL_SUDO)
    {

        //NRF_LOG_HEXDUMP_INFO(p_evt_write->data, p_evt_write->len+10);
        memset(&setting_name, 0, sizeof(setting_name)); //strncpy does not append 0
        strncpy(setting_name, p_evt_write->data, p_evt_write->len);   //cat appends \0 unlike cpy, max data length=20 limited by characteristic

        //check if default name, needs mac append
        if(strcmp(setting_name, "Scale") == 0) //Default name => append MAC suffix
        { 
            ble_gap_addr_t ble_mac_addr;   // BLE MAC address 
            sd_ble_gap_addr_get(&ble_mac_addr); //get MAC
            snprintf(setting_name+5, sizeof(setting_name)-5, " %02x:%02x:%02x\0", ble_mac_addr.addr[2],ble_mac_addr.addr[1],ble_mac_addr.addr[0]);
        }

        //update device name
        ble_gap_conn_sec_mode_t sec_mode;
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
        ret_code_t err_code = sd_ble_gap_device_name_set(&sec_mode,
                                              setting_name,
                                              strlen(setting_name));
        APP_ERROR_CHECK(err_code);

        NRF_LOG_INFO("new name: '%s' len=%d",setting_name, p_evt_write->len);

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }

}
/**@brief Function for handling write events to the save settings characteristic.
 *
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void save_settings_handler(uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    ret_code_t err_code;

    if(user_level>USER_LEVEL_USER)
    {

        NRF_LOG_INFO("save_settings_handler");
        //NRF_LOG_HEXDUMP_INFO(p_evt_write->data, p_evt_write->len);

        m_fstorage_pending = true;

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        auth_reply.params.write.gatt_status=BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        auth_reply.params.write.update = 1;
        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }


}
/**@brief Function for handling write events to the save settings characteristic.
 *
 * @param[in] p_cfs         Instance of configuration service to which the write applies.
 * @param[in] p_evt_write   Write params of the event received from the BLE stack.
 */
static void get_settings_handler(uint16_t conn_handle, ble_cfs_t * p_cfs)
{
    ble_gatts_rw_authorize_reply_params_t auth_reply;
    ret_code_t err_code;

    if(user_level>USER_LEVEL_USER)
    {

        NRF_LOG_INFO("get_settings_handler");


       char updatestring[50] = {0};
       snprintf(updatestring, sizeof(updatestring), "RT=%04d;RN=%04d;TO=%01d;CF=%04d;GR=%01d;", setting_refresh_time,
                                                                                       setting_refresh_notify,
                                                                                       setting_timeout,
                                                                                       setting_correction_factor,
                                                                                       setting_gain_rate);
        
        NRF_LOG_INFO("get settings string length: %d, max allowed: %d", strlen(updatestring), NRF_SDH_BLE_GATT_MAX_MTU_SIZE-2 );

        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
        auth_reply.params.read.gatt_status=BLE_GATT_STATUS_SUCCESS;
        auth_reply.params.read.len = strlen(updatestring);
        //auth_reply.params.read.offset = 0;
        auth_reply.params.read.update = 1;
        auth_reply.params.read.p_data = updatestring;

        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);

    }
    else
    {
        NRF_LOG_INFO("Unauthorised");
        
        memset(&auth_reply, 0, sizeof(auth_reply));
        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
        auth_reply.params.read.gatt_status=BLE_GATT_STATUS_ATTERR_READ_NOT_PERMITTED;
        //auth_reply.params.read.len = 8;
        //auth_reply.params.read.offset = 0;
        //auth_reply.params.read.update = 1;
        //auth_reply.params.read.p_data = updatestring;

        err_code = sd_ble_gatts_rw_authorize_reply(conn_handle, &auth_reply);
        APP_ERROR_CHECK(err_code);
    }


}



/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    ret_code_t         err_code;
    ble_lgs_init_t     lgs_init     = {0};
    ble_cfs_init_t     cfs_init     = {0};

    ble_dfu_buttonless_init_t dfus_init = {0};
    ble_bas_init_t     bas_init;
    ble_dis_init_t     dis_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize DFU service
    
    dfus_init.evt_handler = ble_dfu_evt_handler;

    err_code = ble_dfu_buttonless_init(&dfus_init);
    APP_ERROR_CHECK(err_code);

    // Initialize Login service.
    lgs_init.password_input_handler  = password_input_handler;
    lgs_init.password_change_handler = password_change_handler;
    
    err_code = ble_lgs_init(&m_lgs, &lgs_init);
    APP_ERROR_CHECK(err_code);


    // Initialize Configuration service.
    cfs_init.tare_handler = tare_handler;
    cfs_init.power_off_handler = power_off_handler;
    cfs_init.set_refresh_time_handler = set_refresh_time_handler;
    cfs_init.set_refresh_notify_handler = set_refresh_notify_handler;
    cfs_init.set_turn_off_timeout_handler = set_turn_off_timeout_handler;
    cfs_init.set_correction_handler = set_correction_handler;
    cfs_init.set_gain_rate_handler = set_gain_rate_handler;
    cfs_init.change_name_handler = change_name_handler;
    cfs_init.save_settings_handler = save_settings_handler;
    cfs_init.get_settings_handler = get_settings_handler;

    err_code = ble_cfs_init(&m_cfs, &cfs_init);
    APP_ERROR_CHECK(err_code);

    // Initialize Scale measurement service.
    err_code = ble_sms_init(&m_sms);
    APP_ERROR_CHECK(err_code);



    // Initialize Battery Service.
    memset(&bas_init, 0, sizeof(bas_init));

    // Here the sec level for the Battery Service can be changed/increased.
    bas_init.bl_rd_sec        = SEC_OPEN;
    bas_init.bl_cccd_wr_sec   = SEC_OPEN;
    bas_init.bl_report_rd_sec = SEC_OPEN;

    err_code = ble_bas_init(&m_bas, &bas_init);
    APP_ERROR_CHECK(err_code);

    // Initialize Device Information Service.
    memset(&dis_init, 0, sizeof(dis_init));


    ble_srv_ascii_to_utf8(&dis_init.manufact_name_str,(char *)MANUFACTURER_NAME);
    ble_srv_ascii_to_utf8(&dis_init.model_num_str,    (char *)MODEL_NUMBER);
    //convert hex array to ascii
    uint8_t serial_num_def[6];
    get_serial_number(serial_num_def);

    static uint8_t serial_num_str[20];
    snprintf(serial_num_str, 13, "%02x%02x%02x%02x%02x%02x", serial_num_def[0], serial_num_def[1], serial_num_def[2], serial_num_def[3], serial_num_def[4], serial_num_def[5] );
    ble_srv_ascii_to_utf8(&dis_init.serial_num_str,   (char *)serial_num_str);
    ble_srv_ascii_to_utf8(&dis_init.fw_rev_str,       (char *)FW_REVISION);
    ble_srv_ascii_to_utf8(&dis_init.hw_rev_str,       (char *)HW_REVISION);

    dis_init.dis_char_rd_sec = SEC_OPEN;

    err_code = ble_dis_init(&dis_init);
    APP_ERROR_CHECK(err_code);



}


/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module that
 *          are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply
 *       setting the disconnect_on_fail config parameter, but instead we use the event
 *       handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    ret_code_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    ret_code_t           err_code;

    err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
    APP_ERROR_CHECK(err_code);

}


/**@brief Function for evaluating the value written in CCCD
 *
 * @details This shall be called when there is a write event received from the stack. This
 *          function will evaluate whether or not the peer has enabled notifications and
 *          start/stop timers accordingly.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_write(ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if ((p_evt_write->handle == m_sms.scale_measurement_char_handles.cccd_handle) && (p_evt_write->len == 2))
    {   //Received cccd -> changed notifying status

        
        
        if( true )//user_level>USER_LEVEL_USER)
        {
            //m_is_notification_enabled = ble_srv_is_notification_enabled(p_evt_write->data);
            m_is_notification_enabled = ble_srv_is_indication_enabled(p_evt_write->data);

            if (m_is_notification_enabled)  //start scale related timers as indication is enabled
            {
                NRF_LOG_INFO("Started notifying scale value");

                ret_code_t err_code = app_timer_start(m_timer_refresh_time,
                               APP_TIMER_TICKS(setting_refresh_time),
                               NULL);
                APP_ERROR_CHECK(err_code);
                err_code = app_timer_start(m_timer_refresh_notify,
                               APP_TIMER_TICKS(setting_refresh_notify),
                               NULL);
                APP_ERROR_CHECK(err_code);

            }
            else  //stop scale related timers as indication is disabled
            {
                NRF_LOG_INFO("Stopped notifying scale value");
                ret_code_t err_code = app_timer_stop(m_timer_refresh_time);
                APP_ERROR_CHECK(err_code);
                err_code = app_timer_stop(m_timer_refresh_notify);
                APP_ERROR_CHECK(err_code);

                spi_power_off(); //power off scale

            }


        }
        else
        {
            NRF_LOG_INFO("Indications unauthorised");

        }

    }
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    ret_code_t err_code;
    //NRF_LOG_INFO("BLE event");

    //BLE event, restart power timeout.

    if(setting_timeout!=0)
    {
        err_code = app_timer_stop(m_timer_power_timeout);
        APP_ERROR_CHECK(err_code);
        err_code = app_timer_start(m_timer_power_timeout,
                                   APP_TIMER_TICKS(setting_timeout*60*1000),
                                   NULL);
        APP_ERROR_CHECK(err_code);
    }

    //Blink BLE activity LED
    //and save leds status so they can be turned on again
    nrf_gpio_pin_set(LED_BLE);
    m_led_charging_prev_state     = m_led_charging;
    m_led_battery_full_prev_state = m_led_battery_full;
    led_control_red_green(0,0);

    //Led timer, turns off blue led after a timeout and restores red green
    err_code = app_timer_start(m_timer_led_off,
                               LED_BLE_OFF_TIMEOUT,
                               NULL);
    APP_ERROR_CHECK(err_code);




    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");

            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);




            //start power timeout
            if(setting_timeout!=0)
            {
                err_code = app_timer_start(m_timer_power_timeout,
                                          APP_TIMER_TICKS(setting_timeout*60*1000),
                                          NULL);
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            
            //logout
            user_level = 0;

            //stop timers as user will be disconnected
            err_code = app_timer_stop(m_timer_refresh_time);
            APP_ERROR_CHECK(err_code);
            err_code = app_timer_stop(m_timer_refresh_notify);
            APP_ERROR_CHECK(err_code);
            err_code = app_timer_stop(m_timer_power_timeout);
            APP_ERROR_CHECK(err_code);

            //timer_charger stopped when power disconnected
            //timer_led_off needs to finish so leds return to correct value.

            //turn off scale ADC
            spi_power_off();


            NRF_LOG_INFO("Disconnected");
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            advertising_start();
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle,
                                                   BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
                                                   NULL,
                                                   NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            NRF_LOG_DEBUG("GATT Client Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            NRF_LOG_DEBUG("GATT Server Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_ble_evt);
            break;
/*        
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:// triggered on read if is_defered_read is set on characteristic
            NRF_LOG_INFO("authorize?");
            break;
*/
        default:
            // No implementation needed.
            break;
    }

}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);


    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);


    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}




static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{

    if (m_fstorage_pending == true)
    {   
        m_fstorage_pending = false;
        // must be called from main context, not BLE(SD) event as
        // fstorage flash writing is done by SD and flash is always busy during SD event.
        fstorage_save(&setting_refresh_time, 
                  &setting_refresh_notify, 
                  &setting_timeout,
                  &setting_correction_factor,
                  &setting_gain_rate,
                  setting_password,
                  setting_name);
    }

    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}




/**@brief Function for application main entry.
 */
int main(void)
{
    log_init();
    //Initialize the async SVCI interface to bootloader before any interrupts are enabled.
    
    //TODO uncomment this to enable OTA
    //ret_code_t err_code = ble_dfu_buttonless_async_svci_init();
    //APP_ERROR_CHECK(err_code);
    
    
    power_management_init();
    fstorage_init(&setting_refresh_time,        //initializes variables to values from flash
          &setting_refresh_notify,
          &setting_timeout,
          &setting_correction_factor,
          &setting_gain_rate,
          setting_password,
          setting_name);


    //Override advertising name for debugging
    //snprintf(setting_name, sizeof(setting_name), "scale fw version 0");


    gpio_init();
    timers_init();


    //Initialize MAX77734
    twi_init();


    buttons_init(); //button checks if charger is connected

    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    spi_init(setting_gain_rate);
    passwords_init();


    //Output settings for debugging
    NRF_LOG_INFO("rt: %d",setting_refresh_time);
    NRF_LOG_INFO("ri: %d",setting_refresh_notify);
    NRF_LOG_INFO("to: %d",setting_timeout);
    NRF_LOG_INFO("cf: %d",setting_correction_factor);
    NRF_LOG_INFO("gr: %d",setting_gain_rate);

    NRF_LOG_INFO("pass: %s",setting_password);
    NRF_LOG_INFO("name: %s",setting_name);









    // Start execution.
    advertising_start();
    NRF_LOG_INFO("Started advertising");
    NRF_LOG_INFO("Scale FW version %s",FW_REVISION);

    //force check charger connected status on startup
    button_event_handler(BUTTON_BATT_DETECTED, nrf_gpio_pin_read(BUTTON_BATT_DETECTED)); 





    // Enter main loop.
    for (;;)
    {
        idle_state_handle();
    }

}


