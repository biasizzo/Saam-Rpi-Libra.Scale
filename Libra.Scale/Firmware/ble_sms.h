
#ifndef BLE_SMS_H__
#define BLE_SMS_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@brief   Macro for defining a ble_sms instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_SMS_DEF(_name)                                                                          \
static ble_sms_t _name;                                                                             \
NRF_SDH_BLE_OBSERVER(_name ## _obs, 2, ble_sms_on_ble_evt, &_name)


#ifndef MASTER_UUID_BASE
#define MASTER_UUID_BASE         {0x7B, 0xEC, 0x25, 0xF9, 0x97, 0x16, 0xB2, 0xB9, \
                                          0xE4, 0x11, 0x07, 0xF3, 0x00, 0x00, 0xB8, 0xD2}
#endif

#define SMS_UUID_SERVICE            0x3DE2
#define SMS_UUID_WEIGHT_MEASUREMENT 0x7E74 //INDICATE

// Forward declaration of the ble_sms_t type.
typedef struct ble_sms_s ble_sms_t;


/**@brief LED Button Service structure. This structure contains various status information for the service. */
struct ble_sms_s
{
    uint16_t                    service_handle;                   /**< Handle of Service (as provided by the BLE stack). */
    ble_gatts_char_handles_t    scale_measurement_char_handles;   /**< Handles (INDICATE) Characteristic.) */
    uint8_t                     uuid_type;                        /**< UUID type for the Service. */
    
};


/**@brief Function for initializing the LED Button Service.
 *
 * @param[out] p_sms      LED Button Service structure. This structure must be supplied by
 *                        the application. It is initialized by this function and will later
 *                        be used to identify this particular service instance.
 * @param[in] p_sms_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was initialized successfully. Otherwise, an error code is returned.
 */
uint32_t ble_sms_init(ble_sms_t * p_sms);


/**@brief Function for handling the application's BLE stack events.
 *
 * @details This function handles all events from the BLE stack that are of interest to the LED Button Service.
 *
 * @param[in] p_ble_evt  Event received from the BLE stack.
 * @param[in] p_context  sms Service structure.
 */
void ble_sms_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);


/**@brief Function for sending a button state notification.
 *
 ' @param[in] conn_handle   Handle of the peripheral connection to which the button state notification will be sent.
 * @param[in] p_sms         scale measurement Service structure.
 * @param[in] value.
 *
 * @retval NRF_SUCCESS If the notification was sent successfully. Otherwise, an error code is returned.
 */

uint32_t ble_sms_send_measurement(uint16_t conn_handle, ble_sms_t * p_sms, char * value);


#ifdef __cplusplus
}
#endif

#endif // BLE_sms_H__

/** @} */
