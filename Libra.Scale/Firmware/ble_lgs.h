
#ifndef BLE_LGS_H__
#define BLE_LGS_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@brief   Macro for defining a ble_lgs instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_LGS_DEF(_name)                                                                          \
static ble_lgs_t _name;                                                                             \
NRF_SDH_BLE_OBSERVER(_name ## _obs, 2, ble_lgs_on_ble_evt, &_name)


#ifndef MASTER_UUID_BASE
#define MASTER_UUID_BASE         {0x7B, 0xEC, 0x25, 0xF9, 0x97, 0x16, 0xB2, 0xB9, \
                                          0xE4, 0x11, 0x07, 0xF3, 0x00, 0x00, 0xB8, 0xD2}
#endif

#define LGS_UUID_SERVICE      0x7262
#define LGS_UUID_PASS_INPUT   0x74EC //WRITE
#define LGS_UUID_PASS_CHANGE  0x76A4 //WRITE


// Forward declaration of the ble_lgs_t type.
typedef struct ble_lgs_s ble_lgs_t;

typedef void (*ble_lgs_password_write_handler_t) (uint16_t conn_handle, ble_lgs_t * p_lgs, ble_gatts_evt_write_t const * p_evt_write);

/** @brief Login service init structure. This structure contains all options and data needed for
 *        initialization of the service.*/
typedef struct
{
    ble_lgs_password_write_handler_t password_input_handler;  /**< Event handler to be called when the Characteristic is written. */
    ble_lgs_password_write_handler_t password_change_handler; /**< Event handler to be called when the Characteristic is written. */
} ble_lgs_init_t;

/**@brief Login service structure. This structure contains various status information for the service. */
struct ble_lgs_s
{
    uint16_t                    service_handle;                 /**< Handle of Login service (as provided by the BLE stack). */
    ble_gatts_char_handles_t    pass_input_char_handles;        /**< Handles related to pass input characteristic */
    ble_gatts_char_handles_t    pass_change_char_handles;       /**< Handles related to pass change characteristic */
    uint8_t                     uuid_type;                      /**< UUID type for the Login service. */
    ble_lgs_password_write_handler_t password_input_handler;    /**< Event handler to be called when the Characteristic is written. */
    ble_lgs_password_write_handler_t password_change_handler;   /**< Event handler to be called when the Characteristic is written. */

};


/**@brief Function for initializing the Login service.
 *
 * @param[out] p_lgs      Login service structure. This structure must be supplied by
 *                        the application. It is initialized by this function and will later
 *                        be used to identify this particular service instance.
 * @param[in] p_lgs_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was initialized successfully. Otherwise, an error code is returned.
 */
uint32_t ble_lgs_init(ble_lgs_t * p_lgs, const ble_lgs_init_t * p_lgs_init);


/**@brief Function for handling the application's BLE stack events.
 *
 * @details This function handles all events from the BLE stack that are of interest to the Login service.
 *
 * @param[in] p_ble_evt  Event received from the BLE stack.
 * @param[in] p_context  Login service structure.
 */
void ble_lgs_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);



#ifdef __cplusplus
}
#endif

#endif // BLE_lgs_H__

/** @} */
