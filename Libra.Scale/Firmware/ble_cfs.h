
#ifndef BLE_CFS_H__
#define BLE_CFS_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@brief   Macro for defining a ble_cfs instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_CFS_DEF(_name)                                                                          \
static ble_cfs_t _name;                                                                             \
NRF_SDH_BLE_OBSERVER(_name ## _obs, 2, ble_cfs_on_ble_evt, &_name)


#ifndef MASTER_UUID_BASE
#define MASTER_UUID_BASE         {0x7B, 0xEC, 0x25, 0xF9, 0x97, 0x16, 0xB2, 0xB9, \
                                          0xE4, 0x11, 0x07, 0xF3, 0x00, 0x00, 0xB8, 0xD2}
#endif

#define CFS_UUID_SERVICE                0x4332 
#define CFS_UUID_TARE                   0x6560 // WRITE
#define CFS_UUID_POWER_OFF              0x4800 // WRITE
#define CFS_UUID_SET_REFRESH_TIME       0x4BCA // WRITE
#define CFS_UUID_SET_REFRESH_NOTIFY     0x4BCB // WRITE    *new
#define CFS_UUID_SET_TURN_OFF_TIMEOUT   0x4DAA // WRITE
#define CFS_UUID_GET_SETTINGS           0x5DA4 // READ
#define CFS_UUID_SAVE_SETTINGS          0x5BE2 // WRITE
#define CFS_UUID_SET_CORRECTION         0x517E // WRITE
#define CFS_UUID_SET_GAIN_RATE          0x528E // WRITE    *new
#define CFS_UUID_CHANGE_NAME            0x89BE // WRITE

// Forward declaration of the ble_cfs_t type.
typedef struct ble_cfs_s ble_cfs_t;

typedef void (*ble_cfs_setting_write_handler_t) (uint16_t conn_handle, ble_cfs_t * p_cfs, ble_gatts_evt_write_t const * p_evt_write);
typedef void (*ble_cfs_setting_read_handler_t)  (uint16_t conn_handle, ble_cfs_t * p_cfs);


/** @brief Configuration service init structure. This structure contains all options and data needed for
 *        initialization of the service.*/
typedef struct
{
    ble_cfs_setting_write_handler_t tare_handler;                 /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t power_off_handler;            /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_refresh_time_handler;     /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_refresh_notify_handler;   /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_turn_off_timeout_handler; /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_correction_handler;       /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_gain_rate_handler;        /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t change_name_handler;          /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t save_settings_handler;        /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_read_handler_t  get_settings_handler;         /**< Event handler to be called when the Characteristic is read. */
   
} ble_cfs_init_t;

/**@brief Configuration service structure. This structure contains various status information for the service. */
struct ble_cfs_s
{
    uint16_t                    service_handle;                   /**< Handle of Configuration service (as provided by the BLE stack). */
    ble_gatts_char_handles_t    tare_char_handles;                /**< Handles related to a (WRITE) Characteristic. */
    ble_gatts_char_handles_t    power_off_char_handles;           /**< Handles related to a (WRITE) Characteristic. */
    ble_gatts_char_handles_t    set_refresh_time_char_handles;    /**< Handles related to a (WRITE) Characteristic. */
    ble_gatts_char_handles_t    set_refresh_notify_char_handles;  /**< Handles related to a (WRITE) Characteristic. */
    ble_gatts_char_handles_t    set_turn_off_timeout_char_handles;/**< Handles related to a (WRITE) Characteristic. */
    ble_gatts_char_handles_t    set_correction_char_handles;      /**< Handles related to a (WRITE) Characteristic. */
    ble_gatts_char_handles_t    set_gain_rate_char_handles;       /**< Handles related to a (WRITE) Characteristic. */  
    ble_gatts_char_handles_t    change_name_char_handles;         /**< Handles related to a (WRITE) Characteristic. */
    ble_gatts_char_handles_t    save_settings_char_handles;       /**< Handles related to a (WRITE) Characteristic. */
    ble_gatts_char_handles_t    get_settings_char_handles;        /**< Handles related to the get settings (READ) Characteristic.) */
    uint8_t                     uuid_type;                        /**< UUID type for the Configuration service. */
    
    ble_cfs_setting_write_handler_t tare_handler;                 /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t power_off_handler;            /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_refresh_time_handler;     /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_refresh_notify_handler;   /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_turn_off_timeout_handler; /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_correction_handler;       /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t set_gain_rate_handler;        /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t change_name_handler;          /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_write_handler_t save_settings_handler;        /**< Event handler to be called when the Characteristic is written. */
    ble_cfs_setting_read_handler_t  get_settings_handler;         /**< Event handler to be called when the Characteristic is read. */
   
};


/**@brief Function for initializing the Configuration service.
 *
 * @param[out] p_cfs      Configuration service structure. This structure must be supplied by
 *                        the application. It is initialized by this function and will later
 *                        be used to identify this particular service instance.
 * @param[in] p_cfs_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was initialized successfully. Otherwise, an error code is returned.
 */
uint32_t ble_cfs_init(ble_cfs_t * p_cfs, const ble_cfs_init_t * p_cfs_init);


/**@brief Function for handling the application's BLE stack events.
 *
 * @details This function handles all events from the BLE stack that are of interest to the Configuration service.
 *
 * @param[in] p_ble_evt  Event received from the BLE stack.
 * @param[in] p_context  Configuration service structure.
 */
void ble_cfs_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);


#ifdef __cplusplus
}
#endif

#endif // BLE_cfs_H__

/** @} */
