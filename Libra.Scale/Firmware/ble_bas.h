/** @file
 *
 * @defgroup ble_bas Battery Service
 * @{

 */
#ifndef BLE_BAS_H__
#define BLE_BAS_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Macro for defining a ble_bas instance.
 *
 * @param   _name  Name of the instance.
 * @hideinitializer
 */
#define BLE_BAS_DEF(_name)                          \
    static ble_bas_t _name;                         \
    NRF_SDH_BLE_OBSERVER(_name ## _obs,             \
                         BLE_BAS_BLE_OBSERVER_PRIO, \
                         ble_bas_on_ble_evt,        \
                         &_name)


#ifndef MASTER_UUID_BASE
#define MASTER_UUID_BASE         {0x7B, 0xEC, 0x25, 0xF9, 0x97, 0x16, 0xB2, 0xB9, \
                                          0xE4, 0x11, 0x07, 0xF3, 0x00, 0x00, 0xB8, 0xD2}
#endif

#define BAS_UUID_CUSTOM_BATTERY                0x7A32 


/**@brief Battery Service event. */
typedef struct
{
    uint16_t           conn_handle; /**< Connection handle. */
} ble_bas_evt_t;

// Forward declaration of the ble_bas_t type.
typedef struct ble_bas_s ble_bas_t;

/**@brief Battery Service event handler type. */
typedef void (* ble_bas_evt_handler_t) (ble_bas_t * p_bas, ble_bas_evt_t * p_evt);

/**@brief Battery Service init structure. This contains all options and data needed for
 *        initialization of the service.*/
typedef struct
{
    ble_bas_evt_handler_t  evt_handler;                    /**< Event handler to be called for handling events in the Battery Service. */
    bool                   support_notification;           /**< TRUE if notification of Battery Level measurement is supported. */
    ble_srv_report_ref_t * p_report_ref;                   /**< If not NULL, a Report Reference descriptor with the specified value will be added to the Battery Level characteristic */
    uint8_t                initial_batt_level;             /**< Initial battery level */
    security_req_t         bl_rd_sec;                      /**< Security requirement for reading the BL characteristic value. */
    security_req_t         bl_cccd_wr_sec;                 /**< Security requirement for writing the BL characteristic CCCD. */
    security_req_t         bl_report_rd_sec;               /**< Security requirement for reading the BL characteristic descriptor. */
} ble_bas_init_t;

/**@brief Battery Service structure. This contains various status information for the service. */
struct ble_bas_s
{
    ble_bas_evt_handler_t    evt_handler;               /**< Event handler to be called for handling events in the Battery Service. */
    uint16_t                 service_handle;            /**< Handle of Battery Service (as provided by the BLE stack). */
    ble_gatts_char_handles_t battery_level_handles;     /**< Handles related to the Battery Level characteristic. */
    ble_gatts_char_handles_t custom_battery_level_handles;     /**< Handles related to the custom Battery Level characteristic. */
    uint16_t                 report_ref_handle;         /**< Handle of the Report Reference descriptor. */
    uint8_t                  battery_charging;        /**< Last Battery Level measurement passed to the Battery Service. */
    uint8_t                  battery_charging_done;        /**< Last Battery Level measurement passed to the Battery Service. */
    uint8_t                  uuid_type;                 /**< UUID type for the Configuration service. */
    bool                     is_notification_supported; /**< TRUE if notification of Battery Level is supported. */
};


/**@brief Function for initializing the Battery Service.
 *
 * @param[out]  p_bas       Battery Service structure. This structure will have to be supplied by
 *                          the application. It will be initialized by this function, and will later
 *                          be used to identify this particular service instance.
 * @param[in]   p_bas_init  Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on successful initialization of service, otherwise an error code.
 */
ret_code_t ble_bas_init(ble_bas_t * p_bas, const ble_bas_init_t * p_bas_init);


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @details Handles all events from the BLE stack of interest to the Battery Service.
 *
 * @note For the requirements in the BAS specification to be fulfilled,
 *       ble_bas_battery_level_update() must be called upon reconnection if the
 *       battery level has changed while the service has been disconnected from a bonded
 *       client.
 *
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 * @param[in]   p_context   Battery Service structure.
 */
void ble_bas_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);


/**@brief Function for updating the battery level.
 *
 * @details The application calls this function after having performed a battery measurement.
 *          The battery level characteristic will only be sent to the clients which have
 *          enabled notifications. \ref BLE_CONN_HANDLE_ALL can be used as a connection handle
 *          to send notifications to all connected devices.
 *
 * @param[in]   p_bas          Battery Service structure.
 * @param[in]   battery_level  New battery measurement value (in percent of full capacity).
 * @param[in]   conn_handle    Connection handle.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
ret_code_t ble_bas_battery_level_update(uint16_t    conn_handle, ble_bas_t * p_bas,
                                        uint8_t     battery_level);

ret_code_t ble_bas_battery_level_update_flags(uint16_t    conn_handle, ble_bas_t * p_bas,
                                              uint8_t     charging,
                                              uint8_t     charging_done);


#ifdef __cplusplus
}
#endif

#endif // BLE_BAS_H__

/** @} */
